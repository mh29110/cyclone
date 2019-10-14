/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include "cyc_system_api.h"

#ifdef CY_SYS_WINDOWS
#include <process.h>
#include <Shlwapi.h>
#else
#include <sys/syscall.h>
#include <sys/time.h>
#include <pthread.h>
#include <sched.h>
#endif

#ifdef CY_SYS_MACOS
#include <libproc.h>
#endif

#include <time.h>
#include <chrono>

namespace cyclone
{
namespace sys_api
{

//-------------------------------------------------------------------------------------
pid_t processGetID(void)
{
#ifdef CY_SYS_WINDOWS
	return (pid_t)::GetCurrentProcessId();
#else
	return ::getpid();
#endif
}

//-------------------------------------------------------------------------------------
void processGetModuleName(char* module_name, size_t max_size)
{
#ifdef CY_SYS_WINDOWS
	char process_path_name[MAX_PATH] = { 0 };
	::GetModuleFileName(::GetModuleHandle(0), process_path_name, MAX_PATH);
	strncpy(module_name, ::PathFindFileNameA(process_path_name), max_size);
#else

	#ifdef CY_SYS_MACOS
		char process_path_name[PROC_PIDPATHINFO_MAXSIZE] = { 0 };
		proc_pidpath(processGetID(), process_path_name, PROC_PIDPATHINFO_MAXSIZE);
	#else
		char process_path_name[256] = { 0 };
		if (readlink("/proc/self/exe", process_path_name, 256)<0) {
			strncpy(process_path_name, "unknown", 256);
		}
	#endif
	
	const char* process_name = strrchr(process_path_name, '/');
	if (process_name != 0) process_name++;
	else process_name = "unknown";

	strncpy(module_name, process_name, max_size);
#endif

}

//-------------------------------------------------------------------------------------
struct thread_data_s
{
	thread_id_t tid;
	std::thread thandle;
	thread_function entry_func;
	void* param;
	std::string name;
	bool detached;
	signal_t resume_signal;
};

//-------------------------------------------------------------------------------------
static thread_local thread_data_s* s_thread_data = nullptr;

//-------------------------------------------------------------------------------------
thread_id_t threadGetCurrentID(void)
{
	return s_thread_data == 0 ? std::this_thread::get_id() : s_thread_data->tid;
}

//-------------------------------------------------------------------------------------
thread_id_t threadGetID(thread_t t)
{
	thread_data_s* data = (thread_data_s*)t;
	return data->tid;
}

//-------------------------------------------------------------------------------------
void _thread_entry(thread_data_s* data)
{
	s_thread_data = data;
	signalWait(data->resume_signal);
	signalDestroy(data->resume_signal);
	data->resume_signal = 0;
	
	//set random seed
	srand((uint32_t)::time(0));

	if (data->entry_func)
		data->entry_func(data->param);

	s_thread_data = nullptr;
	if (data->detached) {
		delete data;
	}
}

//-------------------------------------------------------------------------------------
thread_t _thread_create(thread_function func, void* param, const char* name, bool detached)
{
	thread_data_s* data = new thread_data_s;
	data->param = param;
	data->entry_func = func;
	data->detached = detached;
	data->name = name?name:"";
	data->resume_signal = signalCreate();
	data->thandle = std::thread(_thread_entry, data);
	data->tid = data->thandle.get_id();

	//detached thread
	if (data->detached) data->thandle.detach();
	
	//resume thread
	signalNotify(data->resume_signal);
	return data;
}

//-------------------------------------------------------------------------------------
thread_t threadCreate(thread_function func, void* param, const char* name)
{
	return _thread_create(func, param, name, false);
}

//-------------------------------------------------------------------------------------
void threadCreateDetached(thread_function func, void* param, const char* name)
{
	_thread_create(func, param, name, true);
}

//-------------------------------------------------------------------------------------
void threadSleep(int32_t msec)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(msec));
}

//-------------------------------------------------------------------------------------
void threadJoin(thread_t thread)
{
	thread_data_s* data = (thread_data_s*)thread;
	data->thandle.join();

	if (!(data->detached)) {
		delete data;
	}
}

//-------------------------------------------------------------------------------------
const char* threadGetCurrentName(void)
{
	return (s_thread_data == nullptr) ? "<UNNAME>" : s_thread_data->name.c_str();
}

//-------------------------------------------------------------------------------------
void threadYield(void)
{
	std::this_thread::yield();
}

//-------------------------------------------------------------------------------------
mutex_t mutexCreate(void)
{
#ifdef CY_SYS_WINDOWS
	LPCRITICAL_SECTION cs = (LPCRITICAL_SECTION)CY_MALLOC(sizeof(CRITICAL_SECTION));
	::InitializeCriticalSection(cs);
	return cs;
#else
	pthread_mutex_t* pm = (pthread_mutex_t*)CY_MALLOC(sizeof(pthread_mutex_t));
	::pthread_mutex_init(pm, 0);
	return pm;
#endif
}

//-------------------------------------------------------------------------------------
void mutexDestroy(mutex_t m)
{
#ifdef CY_SYS_WINDOWS
	::DeleteCriticalSection(m);
#else
	::pthread_mutex_destroy(m);
	CY_FREE(m);
#endif
}

//-------------------------------------------------------------------------------------
void mutexLock(mutex_t m)
{
#ifdef CY_SYS_WINDOWS
	::EnterCriticalSection(m);
#else
	::pthread_mutex_lock(m);
#endif
}

//-------------------------------------------------------------------------------------
void mutexUnlock(mutex_t m)
{
#ifdef CY_SYS_WINDOWS
	::LeaveCriticalSection(m);
#else
	::pthread_mutex_unlock(m);
#endif
}

//-------------------------------------------------------------------------------------
#ifndef CY_SYS_WINDOWS
struct signal_s
{
	pthread_mutex_t mutex;
	pthread_cond_t	cond;
	atomic_int32_t  predicate;
};
#endif

//-------------------------------------------------------------------------------------
signal_t signalCreate(void)
{
#ifdef CY_SYS_WINDOWS
	return ::CreateEvent(0, FALSE, FALSE, 0);
#else
	signal_s *sig = (signal_s*)CY_MALLOC(sizeof(*sig));
	sig->predicate = 0;
	pthread_mutex_init(&(sig->mutex), 0);
	pthread_cond_init(&(sig->cond), 0);
	return (signal_t)sig;
#endif
}

//-------------------------------------------------------------------------------------
void signalDestroy(signal_t s)
{
#ifdef CY_SYS_WINDOWS
	::CloseHandle(s);
#else
	signal_s* sig = (signal_s*)s;
	pthread_cond_destroy(&(sig->cond));
	pthread_mutex_destroy(&(sig->mutex));
	CY_FREE(sig);
#endif
}

//-------------------------------------------------------------------------------------
void signalWait(signal_t s)
{
#ifdef CY_SYS_WINDOWS
	::WaitForSingleObject(s, INFINITE);
#else
	signal_s* sig = (signal_s*)s;
	pthread_mutex_lock(&(sig->mutex));
	while (0==sig->predicate.load()) {
		pthread_cond_wait(&(sig->cond), &(sig->mutex));
	}
	sig->predicate = 0;
	pthread_mutex_unlock(&(sig->mutex));
#endif
}

//-------------------------------------------------------------------------------------
#ifndef CY_SYS_WINDOWS
bool _signal_unlock_wait(signal_s* sig, uint32_t ms)
{
	const uint64_t kNanoSecondsPerSecond = 1000ll * 1000ll * 1000ll;

	if (sig->predicate.load() == 1) { //It's light!
		sig->predicate = 0;
		return true;
	}

	//need wait...
	if (ms == 0) return  false;	//zero-timeout event state check optimization

	timeval tv;
	gettimeofday(&tv, 0);
	uint64_t nanoseconds = ((uint64_t)tv.tv_sec) * kNanoSecondsPerSecond + ms * 1000 * 1000 + ((uint64_t)tv.tv_usec) * 1000;

	timespec ts;
	ts.tv_sec = (time_t)(nanoseconds / kNanoSecondsPerSecond);
	ts.tv_nsec = (long int)(nanoseconds - ((uint64_t)ts.tv_sec) * kNanoSecondsPerSecond);
	
	//wait...
	while(0 == sig->predicate.load()) {
		if (pthread_cond_timedwait(&(sig->cond), &(sig->mutex), &ts) != 0)
			return false; //time out
	}

	sig->predicate = 0;
	return true;
}
#endif

//-------------------------------------------------------------------------------------
bool signalTimeWait(signal_t s, uint32_t ms)
{
#ifdef CY_SYS_WINDOWS
	return (WAIT_OBJECT_0 == ::WaitForSingleObject(s, ms));
#else
	signal_s* sig = (signal_s*)s;
	if (ms == 0) {
		if (EBUSY == pthread_mutex_trylock(&(sig->mutex)))
			return false;
	}
	else {
		pthread_mutex_lock(&(sig->mutex));
	}

	bool ret = _signal_unlock_wait(sig, ms);

	pthread_mutex_unlock(&(sig->mutex));
	return ret;
#endif
}

//-------------------------------------------------------------------------------------
void signalNotify(signal_t s)
{
#ifdef CY_SYS_WINDOWS
	::SetEvent(s);
#else
	signal_s* sig = (signal_s*)s;
	pthread_mutex_lock(&(sig->mutex));
	sig->predicate = 1;
	pthread_cond_signal(&(sig->cond));
	pthread_mutex_unlock(&(sig->mutex));
#endif
}

//-------------------------------------------------------------------------------------
int64_t timeNow(void)
{
	const int64_t kMicroSecondsPerSecond = 1000ll * 1000ll;

#ifdef CY_SYS_WINDOWS
	SYSTEMTIME ltm;
	GetLocalTime(&ltm);

	struct tm t;
	t.tm_year = ltm.wYear - 1900;
	t.tm_mon = ltm.wMonth - 1;
	t.tm_mday = ltm.wDay;
	t.tm_hour = ltm.wHour;
	t.tm_min = ltm.wMinute;
	t.tm_sec = ltm.wSecond;
	t.tm_isdst = -1;

	int64_t seconds = (int64_t)mktime(&t);
	return seconds*kMicroSecondsPerSecond + ltm.wMilliseconds * 1000;

#else
	struct timeval tv;
	gettimeofday(&tv, 0);
	int64_t seconds = tv.tv_sec;
	return seconds * kMicroSecondsPerSecond + tv.tv_usec;
#endif
}

//-------------------------------------------------------------------------------------
void timeNow(char* time_dest, size_t max_size, const char* format)
{
	time_t local_time = time(0);
	struct tm tm_now;
#ifdef CY_SYS_WINDOWS
	localtime_s(&tm_now, &local_time);
#else
	localtime_r(&local_time, &tm_now);
#endif
	strftime(time_dest, max_size, format, &tm_now);
}

//-------------------------------------------------------------------------------------
int32_t getCPUCounts(void)
{
	const int32_t DEFAULT_CPU_COUNTS = 2;

#ifdef CY_SYS_WINDOWS
	//use GetLogicalProcessorInformation

	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
	DWORD returnLength = 0;
	while (FALSE == GetLogicalProcessorInformation(buffer, &returnLength))
	{
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			if (buffer) CY_FREE(buffer);
			buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)CY_MALLOC(returnLength);
		}
		else
		{
			if (buffer) CY_FREE(buffer);
			return DEFAULT_CPU_COUNTS;
		}
	}

	int32_t cpu_counts = 0;

	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION p = buffer;
	DWORD byteOffset = 0;
	while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength)
	{
		if (p->Relationship == RelationProcessorCore) {
			cpu_counts++;
		}
		byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
		p++;
	}
	CY_FREE(buffer);
	return cpu_counts;
#else
	long int cpu_counts = 0;
	if ((cpu_counts = sysconf(_SC_NPROCESSORS_ONLN)) == -1){
		CY_LOG(L_ERROR, "get cpu counts \"sysconf(_SC_NPROCESSORS_ONLN)\" error, use default(%d)", DEFAULT_CPU_COUNTS);
		return DEFAULT_CPU_COUNTS;
	}
	return (int32_t)cpu_counts;
#endif
}

}
}
