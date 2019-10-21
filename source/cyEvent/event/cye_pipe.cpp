/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>
#include "cye_pipe.h"

#include <fcntl.h>

namespace cyclone
{

//-------------------------------------------------------------------------------------
bool Pipe::constructSocketPipe(pipe_port_t handles[2])
{	
	//
	//https://trac.transmissionbt.com/browser/trunk/libtransmission/trevent.c
	//
	handles[0] = handles[1] = INVALID_SOCKET;

	socket_t s = socket_api::createSocket();
	if (s == INVALID_SOCKET)
	{
		return false;
	}

	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(0);
	serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	for(;;) {
		if (!socket_api::bind(s, serv_addr)) 
			break;
		if (!socket_api::listen(s)) 
			break;
		if (!socket_api::getSockName(s, serv_addr))
			break;
		if ((handles[1] = socket_api::createSocket()) == INVALID_SOCKET)
			break;
		if (!socket_api::setNonBlock(handles[1], true))
			break;
		if (!socket_api::setCloseOnExec(handles[1], true))
			break;
		if (!socket_api::connect(handles[1], serv_addr)) 
			break;
		if ((handles[0] = socket_api::accept(s, 0)) == INVALID_SOCKET) 
			break;
		if (!socket_api::setNonBlock(handles[0], true))
			break;
		if (!socket_api::setCloseOnExec(handles[0], true))
			break;

		socket_api::closeSocket(s);
		return true;
	}

//error case
	if (handles[0] != INVALID_SOCKET)
		socket_api::closeSocket(handles[0]);
	if (handles[1] != INVALID_SOCKET)
		socket_api::closeSocket(handles[1]);
	socket_api::closeSocket(s);
	handles[0] = handles[1] = INVALID_SOCKET;
	return false;
}

//-------------------------------------------------------------------------------------
void Pipe::destroySocketPipe(pipe_port_t handles[2])
{
	if (handles[0] != INVALID_SOCKET) {
		socket_api::closeSocket(handles[0]);
		handles[0] = INVALID_SOCKET;
	}

	if (handles[1] != INVALID_SOCKET) {
		socket_api::closeSocket(handles[1]);
		handles[1] = INVALID_SOCKET;
	}
}

//-------------------------------------------------------------------------------------
Pipe::Pipe()
{
#ifdef CY_SYS_WINDOWS
	if (!constructSocketPipe(m_pipe_fd))
#else
#ifdef CY_HAVE_PIPE2
	if(::pipe2(m_pipe_fd, O_NONBLOCK|O_CLOEXEC)<0)
#else
	if(::pipe(m_pipe_fd)<0 || 
		!socket_api::setNonBlock(m_pipe_fd[0], true) || !socket_api::setCloseOnExec(m_pipe_fd[0], true) ||
		!socket_api::setNonBlock(m_pipe_fd[1], true) || !socket_api::setCloseOnExec(m_pipe_fd[1], true))
#endif
#endif
	{
		CY_LOG(L_FATAL, "create pipe failed!");
	}
}

//-------------------------------------------------------------------------------------
Pipe::~Pipe()
{
#ifdef CY_SYS_WINDOWS
	destroySocketPipe(m_pipe_fd);
#else
	::close(m_pipe_fd[0]);
	::close(m_pipe_fd[1]);
#endif
}

//-------------------------------------------------------------------------------------
ssize_t Pipe::write(const char* buf, size_t len)
{
	return socket_api::write(m_pipe_fd[1], buf, len);
}

//-------------------------------------------------------------------------------------
ssize_t Pipe::read(char* buf, size_t len)
{
	return socket_api::read(m_pipe_fd[0], buf, len);
}

}
