/*
Copyright(C) thecodeway.com
*/
#include <cy_event.h>
#include <cy_network.h>

#include "cyn_tcp_client.h"

namespace cyclone
{

#define RELEASE_EVENT(looper, id) \
	if (id != Looper::INVALID_EVENT_ID) { \
		looper->disableAll(id); \
		looper->deleteEvent(id); \
		id = Looper::INVALID_EVENT_ID; \
	}
//-------------------------------------------------------------------------------------
TcpClient::TcpClient(Looper* looper, void* param)
	: m_socket(INVALID_SOCKET)
	, m_socket_event_id(Looper::INVALID_EVENT_ID)
	, m_retry_timer_id(Looper::INVALID_EVENT_ID)
	, m_looper(looper)
	, m_param(param)
	, m_connection(nullptr)
{
	//looper muste be setted
	assert(looper);

	m_connection_lock = sys_api::mutexCreate();

	m_listener.onConnected = nullptr;
	m_listener.onMessage = nullptr;
	m_listener.onClose = nullptr;
}

//-------------------------------------------------------------------------------------
TcpClient::~TcpClient()
{
	assert(sys_api::threadGetCurrentID() == m_looper->getThreadID());

	sys_api::mutexDestroy(m_connection_lock);
	m_connection_lock = nullptr;

	RELEASE_EVENT(m_looper, m_socket_event_id);
	RELEASE_EVENT(m_looper, m_retry_timer_id);

	if (m_connection) {
		assert(m_connection->get_state() == Connection::kDisconnected);
	}
}

//-------------------------------------------------------------------------------------
bool TcpClient::connect(const Address& addr)
{
	assert(sys_api::threadGetCurrentID() == m_looper->getThreadID());
	assert(m_socket == INVALID_SOCKET);

	sys_api::auto_mutex lock(m_connection_lock);

	//create socket
	m_socket = socket_api::createSocket();
	//set socket to non-block and close-onexec
	socket_api::setNonBlock(m_socket, true);
	socket_api::setCloseOnExec(m_socket, true);
	//set other socket option
	socket_api::setKeepAlive(m_socket, true);
	socket_api::setLinger(m_socket, false, 0);

	m_serverAddr = addr;

	//set event callback
	m_socket_event_id = m_looper->registerEvent(m_socket, Looper::kRead | Looper::kWrite, this,
		std::bind(&TcpClient::_on_socket_read_write, this),
		std::bind(&TcpClient::_on_socket_read_write, this)
		);

	//start connect to server
	if (!socket_api::connect(m_socket, addr.get_sockaddr_in()))
	{
		CY_LOG(L_ERROR, "connect to server error, errno=%d", socket_api::getLastError());
		return false;
	}

	return true;
}

//-------------------------------------------------------------------------------------
Connection::State TcpClient::get_connection_state(void) const
{
	sys_api::auto_mutex lock(m_connection_lock);

	if (m_connection) return m_connection->get_state();
	else return (m_socket==INVALID_SOCKET) ? Connection::kDisconnected : Connection::kConnecting;
}

//-------------------------------------------------------------------------------------
void TcpClient::_on_connect_status_changed(bool timeout)
{
	assert(m_connection == nullptr);

	if (timeout || socket_api::getSocketError(m_socket) != 0) {
		//logic callback
		uint32_t retry_sleep_ms = 0;
		if (m_listener.onConnected) {
			retry_sleep_ms = m_listener.onConnected(shared_from_this(), nullptr, false);
		}
		_abort_connect(retry_sleep_ms);
	}
	else {
		//connect success!
		
		//remove from event system, taked by Connection
		RELEASE_EVENT(m_looper, m_socket_event_id);

		//established the connection
		m_connection = std::make_shared<Connection>(0, m_socket, m_looper, this);

		//bind callback functions
		if (m_listener.onMessage) {
			m_connection->setOnMessageFunction([this](ConnectionPtr conn) {
				m_listener.onMessage(shared_from_this(), conn);
			});
		}

		if(m_listener.onClose) {
			m_connection->setOnCloseFunction([this](ConnectionPtr conn) {
				m_listener.onClose(shared_from_this());
			});
		}

		//send cached message
		if (!m_sendCache.empty()) {
			m_connection->send((const char*)m_sendCache.normalize(), m_sendCache.size());
			m_sendCache.reset();
		}
		//logic callback
		if (m_listener.onConnected) {
			m_listener.onConnected(shared_from_this(), m_connection, true);
		}
	}
}

//-------------------------------------------------------------------------------------
void TcpClient::_abort_connect(uint32_t retry_sleep_ms)
{
	assert(sys_api::threadGetCurrentID() == m_looper->getThreadID());
	assert(get_connection_state() == Connection::kConnecting);

	RELEASE_EVENT(m_looper, m_socket_event_id);
	RELEASE_EVENT(m_looper, m_retry_timer_id);

	//close current socket
	socket_api::closeSocket(m_socket);
	m_socket = INVALID_SOCKET;

	if (retry_sleep_ms>0) {
		//retry connection? create retry the timer
		m_retry_timer_id = m_looper->registerTimeEvent(retry_sleep_ms, this,
			std::bind(&TcpClient::_on_retry_connect_timer, this, std::placeholders::_1));
	}
}

//-------------------------------------------------------------------------------------
void TcpClient::disconnect(void)
{
	assert(sys_api::threadGetCurrentID() == m_looper->getThreadID());
	m_sendCache.reset();
	switch (get_connection_state())
	{
	case Connection::kDisconnected:
		break;

	case Connection::kConnecting:
		_abort_connect(0u);
		break;

	default:
		assert(m_connection != nullptr);
		m_connection->shutdown();
		break;
	}
}

//-------------------------------------------------------------------------------------
void TcpClient::_on_retry_connect_timer(Looper::event_id_t id)
{
	assert(id == m_retry_timer_id);
	assert(m_connection == nullptr);

	//remove the timer
	RELEASE_EVENT(m_looper, m_retry_timer_id);

	//connect again
	if (!connect(m_serverAddr)) {
		//failed at once!, logic callback
		if (m_listener.onConnected) {
			uint32_t retry_sleep_ms = m_listener.onConnected(shared_from_this(), nullptr, false);

			//retry connection?
			if (retry_sleep_ms>0) {
				m_retry_timer_id = m_looper->registerTimeEvent(retry_sleep_ms, this,
					std::bind(&TcpClient::_on_retry_connect_timer, this, std::placeholders::_1));
			}
		}
	}
}

//-------------------------------------------------------------------------------------
void TcpClient::_on_socket_read_write(void)
{
	if (get_connection_state() == Connection::kConnecting) {
		_on_connect_status_changed(false);
	}
}

//-------------------------------------------------------------------------------------
void TcpClient::send(const char* buf, size_t len)
{
	switch (get_connection_state())
	{
	case Connection::kConnecting:
	{
		assert(sys_api::threadGetCurrentID() == m_looper->getThreadID());
		m_sendCache.memcpyInto(buf, len);
	}
		break;
	case Connection::kConnected:
	{
		m_connection->send(buf, len);
	}
		break;
	default:
		break;
	}
	
}

}

