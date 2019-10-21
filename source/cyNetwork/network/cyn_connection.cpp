/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>
#include "cyn_connection.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
Connection::Connection(int32_t id, socket_t sfd, Looper* looper, void* param)
	: m_id(id)
	, m_socket(sfd)
	, m_state(kConnected)
	, m_looper(looper)
	, m_event_id(Looper::INVALID_EVENT_ID)
	, m_param(param)
	, m_readBuf(kDefaultReadBufSize)
	, m_writeBuf(kDefaultWriteBufSize)
	, m_writeBufLock(nullptr)
	, m_max_sendbuf_len(0)
	, m_debuger(nullptr)
{
	//set socket to non-block and close-onexec
	socket_api::setNonBlock(sfd, true);
	socket_api::setCloseOnExec(sfd, true);
	//set other socket option
	socket_api::setKeepAlive(sfd, true);
	socket_api::setLinger(sfd, false, 0);

	//init write buf lock
	m_writeBufLock = sys_api::mutexCreate();

	m_local_addr = Address(false, m_socket); //create local address
	m_peer_addr = Address(true, m_socket); //create peer address

	//set default debug name
	char temp[MAX_PATH] = { 0 };
	snprintf(temp, MAX_PATH, "connection_%d", id);
	m_name = temp;

	//register socket event
	m_event_id = m_looper->registerEvent(m_socket,
		Looper::kRead,			//care read event only
		this,
		std::bind(&Connection::_on_socket_read, this),
		std::bind(&Connection::_on_socket_write, this)
	);
}

//-------------------------------------------------------------------------------------
Connection::~Connection()
{
	_del_debug_value();

	assert(get_state()==kDisconnected);
	assert(m_socket == INVALID_SOCKET);
	assert(m_event_id == Looper::INVALID_EVENT_ID);
	assert(m_writeBufLock == nullptr);
}

//-------------------------------------------------------------------------------------
Connection::State Connection::get_state(void) const
{ 
	return m_state.load(); 
}

//-------------------------------------------------------------------------------------
void Connection::send(const char* buf, size_t len)
{
	if (buf == nullptr || len == 0) return;

	if (sys_api::threadGetCurrentID() == m_looper->getThreadID())
	{
		_send(buf, len);
	}
	else
	{
		if (get_state() != kConnected)
		{
			//log error, give up send message
			CY_LOG(L_ERROR, "send message state error, state=%d", get_state());
			return;
		}

		//write to output buf
		sys_api::auto_mutex lock(m_writeBufLock);

		//write to write buffer
		m_writeBuf.memcpyInto(buf, len);

		//enable write event, wait socket ready
		m_looper->enableWrite(m_event_id);

	}
}

//-------------------------------------------------------------------------------------
bool Connection::_is_writeBuf_empty(void) const
{
	sys_api::auto_mutex lock(m_writeBufLock);
	return  m_writeBuf.empty();
}

//-------------------------------------------------------------------------------------
void Connection::_send(const char* buf, size_t len)
{
	assert(sys_api::threadGetCurrentID() == m_looper->getThreadID());

	bool faultError = false;
	size_t remaining = len;
	ssize_t nwrote = 0;

	if (m_state != kConnected)
	{
		//log error, give up send message
		CY_LOG(L_ERROR, "send message state error, state=%d", get_state());
		return;
	}

	//nothing in write buf, send it diretly
	if (!(m_looper->isWrite(m_event_id)) && _is_writeBuf_empty())
	{
		nwrote = socket_api::write(m_socket, buf, len);
		if (nwrote >= 0)
		{
			remaining = len - (size_t)nwrote;
		}
		else
		{
			nwrote = 0;
			int err = socket_api::getLastError();

			if(!socket_api::isLastErrorWOULDBLOCK())
			{
				CY_LOG(L_ERROR, "socket send error, err=%d", err);

#ifdef CY_SYS_WINDOWS
				if (err == WSAESHUTDOWN || err == WSAENETRESET)
#else
				if (err == EPIPE || err == ECONNRESET)
#endif
				{
					faultError = true;
				}
			}
		}
	}

	if (!faultError && remaining > 0)
	{
		sys_api::auto_mutex lock(m_writeBufLock);

		//write to write buffer
		m_writeBuf.memcpyInto(buf + nwrote, remaining);

		//enable write event, wait socket ready
		m_looper->enableWrite(m_event_id);
	}

	//shutdown if socket work with fault
	if (faultError) {
		m_looper->disableAll(m_event_id);
		shutdown();
	}
}

//-------------------------------------------------------------------------------------
void Connection::shutdown(void)
{
	assert(sys_api::threadGetCurrentID() == m_looper->getThreadID());
	assert(m_state == kConnected || m_state==kDisconnecting);

	//set the state to disconnecting...
	m_state = kDisconnecting;

	//something still working? wait 
	if (m_looper->isWrite(m_event_id) && !_is_writeBuf_empty()) return;
	
	//ok, we can close the socket now
	socket_api::shutdown(m_socket);
	_on_socket_close();
}

//-------------------------------------------------------------------------------------
void Connection::_on_socket_read(void)
{
	assert(sys_api::threadGetCurrentID() == m_looper->getThreadID());

	ssize_t len = m_readBuf.readSocket(m_socket);

	if (len > 0)
	{
		//notify logic layer...
		if (m_onMessage) {
			m_onMessage(shared_from_this());
		}
	}
	else if (len == 0)
	{
		//the connection was closed by peer, close now!
		_on_socket_close();
	}
	else
	{
		//error!
		_on_socket_error();
	}
}

//-------------------------------------------------------------------------------------
void Connection::_on_socket_write(void)
{
	assert(sys_api::threadGetCurrentID() == m_looper->getThreadID());
	assert(m_state == kConnected || m_state == kDisconnecting);
	
	if (m_looper->isWrite(m_event_id))
	{
		sys_api::auto_mutex lock(m_writeBufLock);
		assert(!m_writeBuf.empty());

		if (m_writeBuf.size() > m_max_sendbuf_len) {
			m_max_sendbuf_len = m_writeBuf.size();
		}

		ssize_t len = m_writeBuf.writeSocket(m_socket);
		if (len > 0) {
			if (m_writeBuf.empty()) {
				m_looper->disableWrite(m_event_id);

				//disconnecting? this is the last message send to client, we can shut it down again
				if (m_state == kDisconnecting) {
					shutdown();
				}
			}
		}
		else
		{
			//log error
			CY_LOG(L_ERROR, "write socket error, err=%d", socket_api::getLastError());
		}
	}
}

//-------------------------------------------------------------------------------------
void Connection::_on_socket_close(void)
{
	assert(sys_api::threadGetCurrentID() == m_looper->getThreadID());
	assert(m_state == kConnected || m_state == kDisconnecting);

	ConnectionPtr thisPtr = shared_from_this();

	//disable all event
	m_state = kDisconnected;

	//delete looper event
	m_looper->disableAll(m_event_id);
	m_looper->deleteEvent(m_event_id);
	m_event_id = Looper::INVALID_EVENT_ID;
	
	//logic callback
	if (m_onClose) {
		m_onClose(thisPtr);
	}

	//reset read/write buf
	m_writeBuf.reset();
	m_readBuf.reset();

	//close socket
	socket_api::closeSocket(m_socket);
	m_socket = INVALID_SOCKET;

	//destroy write buf lock
	sys_api::mutexDestroy(m_writeBufLock);
	m_writeBufLock = nullptr;
}

//-------------------------------------------------------------------------------------
void Connection::_on_socket_error(void)
{
	_on_socket_close();
}

//-------------------------------------------------------------------------------------
void Connection::set_name(const char* name)
{
	assert(sys_api::threadGetCurrentID() == m_looper->getThreadID());

	m_name = name;
}

//-------------------------------------------------------------------------------------
void Connection::debug(DebugInterface* debuger)
{
	if (!debuger || !(debuger->isEnable())) return;

	m_debuger = debuger;
	char key_temp[MAX_PATH] = { 0 };

	snprintf(key_temp, MAX_PATH, "Connection:%s:readbuf_capcity", m_name.c_str());
	debuger->updateDebugValue(key_temp, (int32_t)m_readBuf.capacity());

	snprintf(key_temp, MAX_PATH, "Connection:%s:writebuf_capcity", m_name.c_str());
	debuger->updateDebugValue(key_temp, (int32_t)m_writeBuf.capacity());

	snprintf(key_temp, MAX_PATH, "Connection:%s:max_sendbuf_len", m_name.c_str());
	debuger->updateDebugValue(key_temp, (int32_t)m_max_sendbuf_len);
}

//-------------------------------------------------------------------------------------
void Connection::_del_debug_value(void)
{
	if (!m_debuger || !(m_debuger->isEnable())) return;

	char key_name[MAX_PATH] = { 0 };

	snprintf(key_name, MAX_PATH, "Connection:%s:readbuf_capcity", m_name.c_str());
	m_debuger->delDebugValue(key_name);

	snprintf(key_name, MAX_PATH, "Connection:%s:writebuf_capcity", m_name.c_str());
	m_debuger->delDebugValue(key_name);

	snprintf(key_name, MAX_PATH, "Connection:%s:max_sendbuf_len", m_name.c_str());
	m_debuger->delDebugValue(key_name);
}

}
