/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_CORE_LOGGER_H_
#define _CYCLONE_CORE_LOGGER_H_

#include <cyclone_config.h>

namespace cyclone
{

enum LOG_LEVEL
{
	L_TRACE,
	L_DEBUG,
	L_INFO,
	L_WARN,
	L_ERROR,
	L_FATAL,

	L_MAXIMUM_LEVEL,
};

//----------------------
// log api
//----------------------

//log to a disk file
//filename = process_name.date-time24h.hostname.pid.log
// like "test.20150302-1736.server1.63581.log"
// the time part is the time(LOCAL) of first log be written
void diskLog(LOG_LEVEL level, const char* message, ...);

//get current log filename
const char* getLogFileName(void);

//set the a global log level, default is L_TRACE, 
//all the log message lower than this level will be ignored
void setLogThreshold(LOG_LEVEL level);

}

//userful macro
#ifdef CY_ENABLE_LOG
#define CY_LOG cyclone::diskLog
#else
#define CY_LOG (void)
#endif

#endif
