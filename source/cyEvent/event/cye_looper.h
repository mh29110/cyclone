/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_EVENT_LOOPER_H_
#define _CYCLONE_EVENT_LOOPER_H_

#include <cyclone_config.h>
#include <cy_core.h>
#include <event/cye_pipe.h>

namespace cyclone
{

class Looper : noncopyable
{
public:
	typedef uint32_t event_id_t;
	typedef uint32_t event_t;
	static const event_id_t INVALID_EVENT_ID;

	//the value of event_t
	enum {
		kNone = 0,
		kRead	= 1,
		kWrite	= 1<<1,
	};

	typedef std::function<void(event_id_t id, socket_t fd, event_t event, void* param)> event_callback;
	typedef std::function<void(event_id_t id, void* param)> timer_callback;

public:
	//----------------------
	// event operation(NOT thread safe)
	//----------------------

	//// registe event, return channel id
	event_id_t registerEvent(socket_t sockfd,
		event_t type,
		void* param, 
		event_callback _on_read,
		event_callback _on_write);

	//// registe timer event
	event_id_t registerTimeEvent(uint32_t milliSeconds,
		void* param,
		timer_callback _on_timer);

	//// unregister event
	void deleteEvent(event_id_t id);

	//// main loop(reactor process)
	void loop(void);
	//// reactor step
	void step(void);
	//// push stop request
	void pushStopRequest(void);
	//// is quit cmd active
	bool isQuitPending(void) const { return m_quit_cmd.load() != 0; }

	//// update event
	void disableRead(event_id_t id);
	void enableRead(event_id_t id);
	bool isRead(event_id_t id) const;

	void disableWrite(event_id_t id);
	void enableWrite(event_id_t id);
	bool isWrite(event_id_t id) const;

	void disableAll(event_id_t id);

	//----------------------
	// utility functions(NOT thread safe)
	//----------------------
	thread_id_t getThreadID(void) const { return m_current_thread; }
	uint64_t getLoopCounts(void) const { return m_loop_counts; }

	void debug(DebugInterface* debuger, const char* name);

protected:
	Looper();
	virtual ~Looper();

public:
	static Looper* createLooper(void);
	static void destroyLooper(Looper*);

	//----------------------
	// inner data
	//----------------------
protected:
	enum { DEFAULT_CHANNEL_BUF_COUNTS = 16 };

	struct channel_s
	{
		event_id_t id;
		socket_t fd;
		event_t event;
		void *param;
		bool active;
		bool timer;

		event_callback on_read;
		event_callback on_write;

		event_id_t next;
		event_id_t prev;	//only used in select looper
	};
	typedef std::vector< channel_s > channel_buffer;
	typedef std::vector< event_id_t > channel_list;

	channel_buffer m_channelBuffer;	//all event buf
	event_id_t m_free_head;			//free list head in event buf
	int32_t m_active_channel_counts;
	uint64_t m_loop_counts;

	thread_id_t m_current_thread;

	sys_api::mutex_t m_lock;

	Pipe* m_inner_pipe;	//pipe to push loop continue
	atomic_int32_t m_inner_pipe_touched;
	atomic_int32_t m_quit_cmd;

	/// Polls the I/O events.
	virtual void _poll(
		channel_list& readChannelList,
		channel_list& writeChannelList,
		bool block) = 0;
	/// Changes the interested I/O events.
	virtual void _updateChannelAddEvent(channel_s& channel, event_t type) = 0;
	virtual void _updateChannelRemoveEvent(channel_s& channel, event_t type) = 0;

	/// for timer
	struct timer_s
	{
		timer_callback on_timer;
		void* param;
        uint32_t milli_seconds;

#ifdef CY_SYS_WINDOWS
		Pipe pipe;
		HANDLE  htimer;
#else
#endif
	};
	
	static void _onTimerEventCallback(event_id_t id, socket_t fd, event_t event, void* param);

#ifdef CY_SYS_WINDOWS
	static void __stdcall _onWindowsTimer(PVOID param, BOOLEAN timer_or_wait_fired);
#endif

	//inner pipe functions
	void _touchInnerPipe(void);
	static void _onInnerPipeTouched(event_id_t id, socket_t fd, event_t event, void* param);

private:
	event_id_t _getFreeSlot(void);
};

}

#endif

