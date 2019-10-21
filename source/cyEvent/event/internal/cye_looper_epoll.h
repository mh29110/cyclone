/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_EVENT_LOOPER_EPOLL_H_
#define _CYCLONE_EVENT_LOOPER_EPOLL_H_

#include <cy_core.h>
#include <event/cye_looper.h>

#ifdef CY_HAVE_EPOLL
#include <sys/epoll.h>

namespace cyclone
{

class Looper_epoll : public Looper
{
public:
	/// Polls the I/O events.
	virtual void _poll( 
		channel_list& readChannelList,
		channel_list& writeChannelList,
		bool block);
	/// Changes the interested I/O events.
	virtual void _updateChannelAddEvent(channel_s& channel, event_t event);
	virtual void _updateChannelRemoveEvent(channel_s& channel, event_t event);

private:
	typedef std::vector<struct epoll_event> event_vector;

	event_vector m_events;
	int m_eoll_fd;

private:
	bool _set_event(channel_s& channel, int operation, uint32_t events);

public:
	Looper_epoll();
	virtual ~Looper_epoll();
};

}

#endif

#endif
