/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_CORE_SOCKET_API_H_
#define _CYCLONE_CORE_SOCKET_API_H_

#include <cyclone_config.h>

namespace cyclone
{
namespace socket_api
{

//// init global socket data( call WSAStartup at windows system)
void globalInit(void);

/// Creates a blocking socket file descriptor, return INVALID_SOCKET if failed
socket_t createSocket(void);

/// Close socket
void closeSocket(socket_t s);

/// enable/disable socket non-block mode
bool setNonBlock(socket_t s, bool enable);

/// enable/disable socket close-on-exec mode
bool setCloseOnExec(socket_t s, bool enable);

/// bind a local name to a socket
bool bind(socket_t s, const struct sockaddr_in& addr);

/// listen for connection
bool listen(socket_t s);

/// accept a connection on a socket, return INVALID_SOCKET if failed
socket_t accept(socket_t s, struct sockaddr_in* addr);

/// initiate a connection on a socket
bool connect(socket_t s, const struct sockaddr_in& addr);

/// write to socket file desc
ssize_t write(socket_t s, const char* buf, size_t len);

/// read from a socket file desc
ssize_t read(socket_t s, void *buf, size_t len);

/// shutdown read and write part of a socket connection
bool shutdown(socket_t s);

/// convert ip address (like "123.123.123.123") to in_addr
bool inet_pton(const char* ip, struct in_addr& a);

/// convert in_addr to ip address
bool inet_ntop(const struct in_addr& a, char *dst, socklen_t size);

/// socket operation
bool setSockOpt(socket_t s, int level, int optname, const void *optval, size_t optlen);

/// Enable/disable SO_REUSEADDR
bool setReuseAddr(socket_t s, bool on);

/// Enable/disable SO_REUSEPORT
bool setReusePort(socket_t s, bool on);

/// Enable/disable SO_KEEPALIVE
bool setKeepAlive(socket_t s, bool on);

/// Enable/disable TCP_NODELAY
bool setNoDelay(socket_t s, bool on);

/// Set socket SO_LINGER
bool setLinger(socket_t s, bool on, uint16_t linger_time);

/// get socket error
int getSocketError(socket_t sockfd);

/// get local name of socket
bool getSockName(socket_t s, struct sockaddr_in& addr);

/// get peer name of socket
bool getPeerName(socket_t s, struct sockaddr_in& addr);

///resolve hostname to IP address, not changing port or sin_family
bool resolveHostName(const char* hostname, struct sockaddr_in& addr);

/// big-endian/little-endian convert
uint16_t ntoh_16(uint16_t x);
uint32_t ntoh_32(uint32_t x);

//// get last socket error
int getLastError(void);

//// is lasterror  WOULDBLOCK
bool isLastErrorWOULDBLOCK(void);

}
}

#endif
