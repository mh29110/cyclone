/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_CORE_THREAD_API_H_
#define _CYCLONE_CORE_THREAD_API_H_

#include <cyclone_config.h>

#ifndef CY_SYS_WINDOWS
#include <pthread.h>
#endif

typedef void* thread_t;
typedef std::thread::id thread_id_t;

namespace cyclone
{
namespace sys_api
{
//----------------------
// process functions
//----------------------

//// get current process id
pid_t processGetID(void);

//// get current process name
void processGetModuleName(char* module_name, size_t max_size);

//----------------------
// thread functions
//----------------------
	
//// thread entry function
typedef std::function<void(void*)> thread_function;

//// get current thread id
thread_id_t threadGetCurrentID(void);

//// get the system id of thread
thread_id_t threadGetID(thread_t t);

//// create a new thread(use threadJoin to release resources)
thread_t threadCreate(thread_function func, void* param, const char* name);

//// create a new thread(all thread resources will be released automatic)
void threadCreateDetached(thread_function func, void* param, const char* name);

//// sleep in current thread(milliseconds)
void threadSleep(int32_t msec);

//// wait the thread to terminate
void threadJoin(thread_t t);

//// get current thread name
const char* threadGetCurrentName(void);

//// yield the processor
void threadYield(void);

//----------------------
// mutex functions
//----------------------
#ifdef CY_SYS_WINDOWS
typedef LPCRITICAL_SECTION	mutex_t;
#else
typedef pthread_mutex_t* mutex_t;
#endif

/// create a mutex
mutex_t mutexCreate(void);

/// destroy a mutex
void mutexDestroy(mutex_t m);

/// lock mutex(wait other owner unlock)
void mutexLock(mutex_t m);

/// unlock mutex
void mutexUnlock(mutex_t m);

/// auto lock
struct auto_mutex
{
	auto_mutex(mutex_t m) : _m(m) { mutexLock(_m); }
	~auto_mutex() { mutexUnlock(_m); }
	mutex_t _m;
};

//----------------------
// signal/semaphone functions
//----------------------

#ifdef CY_SYS_WINDOWS
typedef HANDLE	signal_t;
#else
typedef void*	signal_t;
#endif

//// create a signal
signal_t signalCreate(void);

//// destroy a signal
void signalDestroy(signal_t s);

//// wait a signal inifinite
void signalWait(signal_t s);

//// wait a signal in [t] millisecond(second*1000), return true immediately if the signal is lighted, if false if timeout or other error
bool signalTimeWait(signal_t s, uint32_t ms);

//// light the signal
void signalNotify(signal_t s);

//----------------------
// time functions
//----------------------

//// get time in microseconds(second*1000*1000) from Epoch
int64_t timeNow(void);

/// get time in format string(strftime)
void timeNow(char* time_dest, size_t max_size, const char* format);

//----------------------
// utility functions
//----------------------
int32_t getCPUCounts(void);

}
}
#endif
