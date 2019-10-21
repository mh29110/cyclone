/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_EVENT_PIPE_H_
#define _CYCLONE_EVENT_PIPE_H_

#include <cyclone_config.h>

namespace cyclone
{

typedef socket_t pipe_port_t;

class Pipe : noncopyable
{
public:
	pipe_port_t getReadPort(void) { return m_pipe_fd[0]; }
	pipe_port_t getWritePort(void) { return m_pipe_fd[1]; }

	ssize_t write(const char* buf, size_t len);
	ssize_t read(char* buf, size_t len);

	static bool constructSocketPipe(pipe_port_t handles[2]);
	static void destroySocketPipe(pipe_port_t handles[2]);

private:
	pipe_port_t m_pipe_fd[2];

public:
	Pipe();		//build pipe
	~Pipe();
};

}

#endif
