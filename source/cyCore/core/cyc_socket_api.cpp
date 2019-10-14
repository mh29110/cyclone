/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include "cyc_socket_api.h"

#ifdef CY_SYS_WINDOWS
#include <io.h>
#else
#include <netdb.h>
#include <netinet/tcp.h>
#endif
#include <fcntl.h>

//
// Winsock Reference https://msdn.microsoft.com/en-us/library/ms741416(v=vs.85).aspx
//

namespace cyclone
{
namespace socket_api
{

//-------------------------------------------------------------------------------------
#ifdef CY_SYS_WINDOWS
class _auto_init_win_socke_s
{
public:
	_auto_init_win_socke_s() {
		WSADATA wsadata;
		WSAStartup(MAKEWORD(2, 1), &wsadata);
	}
};
static const _auto_init_win_socke_s& AUTO_INIT_WIN_SOCKET()
{
	static _auto_init_win_socke_s s;
	return s;
}

#endif

//-------------------------------------------------------------------------------------
void globalInit(void)
{
#ifdef CY_SYS_WINDOWS
	AUTO_INIT_WIN_SOCKET();
#endif
}

//-------------------------------------------------------------------------------------
socket_t createSocket(void)
{
#ifdef CY_SYS_WINDOWS
	AUTO_INIT_WIN_SOCKET();
#endif

	socket_t sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == INVALID_SOCKET)
	{
		CY_LOG(L_FATAL, "socket_api::createSocket, err=%d", getLastError());
	}

	return sockfd;
}

//-------------------------------------------------------------------------------------
void closeSocket(socket_t s)
{
#ifdef CY_SYS_WINDOWS
	if (SOCKET_ERROR == ::closesocket(s))
#else
	if (SOCKET_ERROR == ::close(s))
#endif
	{
		CY_LOG(L_ERROR, "socket_api::closeSocket, err=%d", getLastError());
	}
}

//-------------------------------------------------------------------------------------
bool setNonBlock(socket_t s, bool enable)
{
#ifdef CY_SYS_WINDOWS
	//set socket to nonblock
	unsigned long nonblock = enable ? 1 : 0;
	if (::ioctlsocket(s, FIONBIO, &nonblock) != 0)
#else
	int32_t flags = ::fcntl(s, F_GETFL, 0);
	if (SOCKET_ERROR == flags ||
		SOCKET_ERROR == ::fcntl(s, F_SETFL, enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)))
#endif
	{
		CY_LOG(L_FATAL, "socket_api::setNonBlock, err=%d", getLastError());
		return false;
	}

	return true;
}

//-------------------------------------------------------------------------------------
bool setCloseOnExec(socket_t s, bool enable)
{
#ifdef CY_SYS_WINDOWS
	(void)s;
	(void)enable;
	//NOT SUPPORT (I'm not sure...)
#else
	// close-on-exec
	int32_t flags = ::fcntl(s, F_GETFD, 0);
	if (SOCKET_ERROR == flags ||
		SOCKET_ERROR == ::fcntl(s, F_SETFD, enable ? (flags | FD_CLOEXEC) : (flags & ~FD_CLOEXEC)))
	{
		CY_LOG(L_FATAL, "socket_api::setCloseOnExec, err=%d", getLastError());
		return false;
	}
#endif
	return true;
}

//-------------------------------------------------------------------------------------
bool bind(socket_t s, const struct sockaddr_in& addr)
{
	if (SOCKET_ERROR == ::bind(s, (const sockaddr*)(&addr), static_cast<socklen_t>(sizeof addr)))
	{
		CY_LOG(L_FATAL, "socket_api::bind, err=%d", getLastError());
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool listen(socket_t s)
{
	if(SOCKET_ERROR == ::listen(s, SOMAXCONN))
	{
		CY_LOG(L_FATAL, "socket_api::listen, err=%d", getLastError());
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool connect(socket_t s, const struct sockaddr_in& addr)
{
	if (::connect(s, (const sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		int lasterr = getLastError();
#ifdef CY_SYS_WINDOWS
		//non-block socket
		if(lasterr == WSAEWOULDBLOCK) return true; 
#else
		//non-block socket
		if (lasterr == EINPROGRESS ||
			lasterr == EINTR ||
			lasterr == EISCONN) return true; 
#endif

		CY_LOG(L_FATAL, "socket_api::connect, err=%d", getLastError());
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool inet_pton(const char* ip, struct in_addr& a)
{
#ifdef CY_SYS_WINDOWS
	AUTO_INIT_WIN_SOCKET();
	if(::InetPton(AF_INET, ip, &a) != 1)
#else
	if (::inet_pton(AF_INET, ip, &a) != 1)
#endif
	{
		CY_LOG(L_FATAL, "socket_api::inet_pton, err=%d", getLastError());
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool inet_ntop(const struct in_addr& a, char *dst, socklen_t size)
{
#ifdef CY_SYS_WINDOWS
	AUTO_INIT_WIN_SOCKET();
	in_addr t;
	memcpy(&t, &a, sizeof(t));
	if (::InetNtop(AF_INET, &t, dst, size) == 0)
#else
	if (::inet_ntop(AF_INET, &a, dst, size) == 0)
#endif
	{
		CY_LOG(L_FATAL, "socket_api::inet_ntop, err=%d", getLastError());
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
ssize_t write(socket_t s, const char* buf, size_t len)
{
#ifdef CY_SYS_WINDOWS
	ssize_t _len = (ssize_t)::send(s, buf, (int32_t)len, MSG_DONTROUTE);
#else
	ssize_t _len = (ssize_t)::write(s, buf, len);
#endif
	return _len;
}

//-------------------------------------------------------------------------------------
ssize_t read(socket_t s, void *buf, size_t len)
{
#ifdef CY_SYS_WINDOWS
	ssize_t _len = (ssize_t)::recv(s, (char*)buf, (int32_t)len, 0);
#else
	ssize_t _len = (ssize_t)::read(s, buf, len);
#endif

	return _len;
}

//-------------------------------------------------------------------------------------
bool shutdown(socket_t s)
{
#ifdef CY_SYS_WINDOWS
	if (SOCKET_ERROR == ::shutdown(s, SD_BOTH))
#else
	if (SOCKET_ERROR == ::shutdown(s, SHUT_RDWR))
#endif
	{
		CY_LOG(L_FATAL, "socket_api::shutdown, err=%d", getLastError());
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool setSockOpt(socket_t s, int level, int optname, const void *optval, size_t optlen)
{
	if (SOCKET_ERROR == ::setsockopt(s, level, optname, (const char*)optval, (socklen_t)optlen))
	{
		CY_LOG(L_ERROR, "socket_api::setSockOpt, level=%d, optname=%d, err=%d", getLastError());
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool setReuseAddr(socket_t s, bool on)
{
	int optval = on ? 1 : 0;
	return setSockOpt(s, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof optval));
}

//-------------------------------------------------------------------------------------
bool setReusePort(socket_t s, bool on)
{
#ifndef SO_REUSEPORT
	(void)s;
	(void)on;
	//NOT SUPPORT 
	CY_LOG(L_WARN, "socket_api::set_reuse_port, System do NOT support REUSEPORT issue!");
	return false;
#else
	int optval = on ? 1 : 0;
	return setSockOpt(s, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof optval));
#endif
}

//-------------------------------------------------------------------------------------
bool setKeepAlive(socket_t s, bool on)
{
	int optval = on ? 1 : 0;
	return setSockOpt(s, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof optval));
}

//-------------------------------------------------------------------------------------
bool setNoDelay(socket_t s, bool on)
{
	int optval = on ? 1 : 0;
	return setSockOpt(s, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof optval));
}

//-------------------------------------------------------------------------------------
bool setLinger(socket_t s, bool on, uint16_t linger_time)
{
	struct linger linger_;

	linger_.l_onoff = on; // && (linger_time > 0)) ? 1 : 0;
	linger_.l_linger = on ? linger_time : 0;

	return setSockOpt(s, SOL_SOCKET, SO_LINGER, &linger_, sizeof(linger_));
}

//-------------------------------------------------------------------------------------
int getSocketError(socket_t sockfd)
{
	int optval;
	socklen_t optlen = sizeof(optval);

	if (SOCKET_ERROR == ::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen))
	{
		return errno;
	}
	else
	{
		return optval;
	}
}

//-------------------------------------------------------------------------------------
socket_t accept(socket_t s, struct sockaddr_in* addr)
{
	socklen_t addrlen = static_cast<socklen_t>(sizeof(sockaddr_in));
	socket_t connfd = ::accept(s, (struct sockaddr *)addr, addr ? (&addrlen) : 0);

	if (connfd == INVALID_SOCKET)
	{
		CY_LOG(L_FATAL, "socket_api::accept, err=%d", getLastError());
	}
	return connfd;
}

//-------------------------------------------------------------------------------------
bool getSockName(socket_t s, struct sockaddr_in& addr)
{
	socklen_t addrlen = static_cast<socklen_t>(sizeof addr);
	memset(&addr, 0, sizeof addr);

	if (SOCKET_ERROR == ::getsockname(s, (struct sockaddr*)(&addr), &addrlen))
	{
		CY_LOG(L_FATAL, "socket_api::getSockName, err=%d", getLastError());
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool getPeerName(socket_t s, struct sockaddr_in& addr)
{
	socklen_t addrlen = static_cast<socklen_t>(sizeof addr);
	memset(&addr, 0, sizeof addr);

	if (SOCKET_ERROR == ::getpeername(s, (struct sockaddr*)(&addr), &addrlen) )
	{
		CY_LOG(L_FATAL, "socket_api::getPeerName, err=%d", getLastError());
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool resolveHostName(const char* hostname, struct sockaddr_in& addr)
{
	socket_api::globalInit();

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	struct addrinfo *result = nullptr;
	if (getaddrinfo(hostname, nullptr, &hints, &result) != 0) {
		CY_LOG(L_ERROR, "socket_api::resolve %s failed, errno=%d", hostname, socket_api::getLastError());
		return false;
	}

	bool find_ipv4_address = false;
	for (auto ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
		if (ptr->ai_family != AF_INET) continue;

		//AF_INET (IPv4)
		addr.sin_addr = ((struct sockaddr_in*) ptr->ai_addr)->sin_addr;
		find_ipv4_address = true;
		break;
	}

	if (!find_ipv4_address) {
		CY_LOG(L_ERROR, "socket_api::resolve %s failed, IPV4 address not found", hostname);
	}

	freeaddrinfo(result);
	return find_ipv4_address;
}

//-------------------------------------------------------------------------------------
uint16_t ntoh_16(uint16_t x)
{
#ifdef CY_SYS_LINUX
	return be16toh(x);
#else
	return ntohs(x);
#endif
}

//-------------------------------------------------------------------------------------
uint32_t ntoh_32(uint32_t x)
{
#ifdef CY_SYS_LINUX
	return be32toh(x);
#else
	return ntohl(x);
#endif
}

//-------------------------------------------------------------------------------------
int getLastError(void)
{
#ifdef CY_SYS_WINDOWS
	return ::WSAGetLastError();
#else
	return errno;
#endif
}

//-------------------------------------------------------------------------------------
bool isLastErrorWOULDBLOCK(void)
{
	//This error is returned from operations on nonblocking sockets that cannot be completed immediately
#ifdef CY_SYS_WINDOWS
	return getLastError() == WSAEWOULDBLOCK;
#else
	return errno==EWOULDBLOCK;
#endif

}

}
}