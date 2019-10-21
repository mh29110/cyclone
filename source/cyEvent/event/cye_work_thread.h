/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_EVENT_WORK_THREAD_H_
#define _CYCLONE_EVENT_WORK_THREAD_H_

#include "core/cyc_lf_queue.h"

namespace cyclone
{
//pre-define
class Packet;

class WorkThread : noncopyable
{
public:
	typedef std::function<bool(void)> StartCallback;
	typedef std::function<void(Packet*)> MessageCallback;

public:
	enum { MESSAGE_HEAD_SIZE = 4 };

	//// run thread
	void start(const char* name);

	//// set callback function
	void setOnStartFunction(StartCallback func) { m_onStart = func; }
	void setOnMessageFunction(MessageCallback func) { m_onMessage = func; }

	//// send message to this work thread (thread safe)
	void sendMessage(uint16_t id, uint16_t size, const char* message);
	void sendMessage(const Packet* message);
	void sendMessage(const Packet** message, int32_t counts);

	//// get work thread looper (thread safe)
	Looper* getLooper(void) const { return m_looper; }

	//// get work thread name (thread safe)
	const char* getName(void) const { return m_name.c_str(); }

	//// join work thread(thread safe)
	void join(void);

private:
	std::string		m_name;
	thread_t		m_thread;
	Looper*			m_looper;
	Pipe			m_pipe;

	typedef LockFreeQueue<Packet*> MessageQueue;
	MessageQueue		m_message_queue;

	StartCallback	m_onStart;
	MessageCallback	m_onMessage;

private:
	/// work thread param
	struct work_thread_param
	{
		WorkThread*		_this;
		atomic_int32_t	_ready;
	};
	/// work thread function
	void _workThread(void* param);

	//// on work thread receive message
	void _onMessage(void);

public:
	WorkThread();
	virtual ~WorkThread();
};

}

#endif
