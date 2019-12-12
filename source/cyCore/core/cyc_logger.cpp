/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include "cyc_logger.h"

#ifdef CY_SYS_WINDOWS
#include <Shlwapi.h>
#include <direct.h>
#elif defined(CY_SYS_MACOS)
#include <libproc.h>
#else
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace cyclone
{

#define LOG_PATH		"./logs/"

//-------------------------------------------------------------------------------------
struct DiskLogFile
{
#ifndef _MAX_PATH
#define _MAX_PATH (260)
#endif

	char m_fileName[_MAX_PATH];
	sys_api::mutex_t m_lock;
	const char* m_levelName[L_MAXIMUM_LEVEL];
	LOG_LEVEL m_levelThreshold;
	bool m_bLogPathCreated;

	DiskLogFile() 
	{
#ifdef CY_SYS_WINDOWS
		socket_api::globalInit();
#endif
		//get process name
		char process_name[256] = { 0 };
		sys_api::processGetModuleName(process_name, 256);
		
		//get host name
		char host_name[256];
		::gethostname(host_name, 256);

		//get process id
		pid_t process_id = sys_api::processGetID();

		//log filename patten
		char name_patten[256] = { 0 };
		snprintf(name_patten, 256, LOG_PATH"%s.%%Y%%m%%d-%%H%%M%%S.%s.%d.log", process_name, host_name, process_id);
		sys_api::localTimeNow(m_fileName, 256, name_patten);

		//create lock
		m_lock = sys_api::mutexCreate();

		//default level(all level will be writed)
		m_levelThreshold = L_TRACE;

		//log path didn't created
		m_bLogPathCreated = false;

		//set level name
		m_levelName[L_TRACE] = "[T]";
		m_levelName[L_DEBUG] = "[D]";
		m_levelName[L_INFO]  = "[I]";
		m_levelName[L_WARN]  = "[W]";
		m_levelName[L_ERROR] = "[E]";
		m_levelName[L_FATAL] = "[F]";
	}
};

//-------------------------------------------------------------------------------------
static DiskLogFile& _getDiskLog(void)
{
	static DiskLogFile thefile;
	return thefile;
}

//-------------------------------------------------------------------------------------
const char* getLogFileName(void)
{
	DiskLogFile& thefile = _getDiskLog();
	return thefile.m_fileName;
}

//-------------------------------------------------------------------------------------
void setLogThreshold(LOG_LEVEL level)
{
	assert(level >= 0 && level <= L_MAXIMUM_LEVEL);
	if (level < 0 || level > L_MAXIMUM_LEVEL)return;

	DiskLogFile& thefile = _getDiskLog();
	sys_api::auto_mutex guard(thefile.m_lock);

	thefile.m_levelThreshold = level;
}

//-------------------------------------------------------------------------------------
void diskLog(LOG_LEVEL level, const char* message, ...)
{
	assert(level >= 0 && level < L_MAXIMUM_LEVEL);
	if (level < 0 || level >= L_MAXIMUM_LEVEL)return;

	DiskLogFile& thefile = _getDiskLog();
	sys_api::auto_mutex guard(thefile.m_lock);

	//check the level
	if (level < thefile.m_levelThreshold) return;

	//check dir
#ifdef CY_SYS_WINDOWS
	if (!thefile.m_bLogPathCreated && PathFileExists(LOG_PATH)!=TRUE) {
		if (0 == CreateDirectory(LOG_PATH, NULL)) 
#else
	if (!thefile.m_bLogPathCreated && access(LOG_PATH, F_OK)!=0) {
		if (mkdir(LOG_PATH, 0755) != 0) 
#endif
		{
			//create log path failed!
			return;
		}
		thefile.m_bLogPathCreated = true;
	}

	FILE* fp = fopen(thefile.m_fileName, "a");
	if (fp == 0) {
		//create the log file first
		fp = fopen(thefile.m_fileName, "w");
	}
	if (fp == 0) return;

	char timebuf[32] = { 0 };
	sys_api::localTimeNow(timebuf, 32, "%Y_%m_%d-%H:%M:%S");

	static const int32_t STATIC_BUF_LENGTH = 2048;

	char szTemp[STATIC_BUF_LENGTH] = { 0 };
	char* p = szTemp;
	va_list ptr; va_start(ptr, message);
	int len = vsnprintf(p, STATIC_BUF_LENGTH, message, ptr);
	if (len < 0) {
		va_start(ptr, message);
		len = vsnprintf(0, 0, message, ptr);
		if (len > 0) {
			p = (char*)CY_MALLOC((size_t)(len + 1));
			va_start(ptr, message);
			vsnprintf(p, (size_t)len + 1, message, ptr);
			p[len] = 0;
		}
	}
	else if (len >= STATIC_BUF_LENGTH) {
		p = (char*)CY_MALLOC((size_t)(len + 1));
		va_start(ptr, message);
		vsnprintf(p, (size_t)len + 1, message, ptr);
		p[len] = 0;
	}
	va_end(ptr);

	fprintf(fp, "%s %s [%s] %s\n",
		timebuf, 
		thefile.m_levelName[level],
		sys_api::threadGetCurrentName(),
		p);
	fclose(fp);

	//print to stand output last
	fprintf(level >= L_ERROR ? stderr : stdout, "%s %s [%s] %s\n",
		timebuf,
		thefile.m_levelName[level],
		sys_api::threadGetCurrentName(),
		p);

	if (p != szTemp) {
		CY_FREE(p);
	}
}



}
