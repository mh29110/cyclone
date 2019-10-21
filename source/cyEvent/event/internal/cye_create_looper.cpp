/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include <cy_event.h>

#include "cye_looper_epoll.h"
#include "cye_looper_select.h"
#include "cye_looper_kqueue.h"

namespace cyclone
{

//-------------------------------------------------------------------------------------
Looper* Looper::createLooper(void)
{
#if (CY_POLL_TECH==CY_POLL_EPOLL)
	return new Looper_epoll();
#elif (CY_POLL_TECH == CY_POLL_KQUEUE)
	return new Looper_kqueue();
#else
	return new Looper_select();
#endif
}

//-------------------------------------------------------------------------------------
void Looper::destroyLooper(Looper* looper)
{
	delete looper;
}

}
