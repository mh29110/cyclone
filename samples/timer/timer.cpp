#include <cy_core.h>
#include <cy_event.h>
#include <cy_network.h>

using namespace cyclone;

//-------------------------------------------------------------------------------------
void _timerFunction(uint32_t id, void* param) {
	Looper* looper = (Looper*)param;
	CY_LOG(L_INFO, "timer%d:pushStopRequest", id);
	looper->pushStopRequest();
}

//-------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	Looper* looper = Looper::createLooper();

	int32_t counts = 0;

	uint32_t id1 = looper->registerTimeEvent(1000-1, 0, [&counts] (uint32_t id, void*)  {
		CY_LOG(L_INFO, "timer%d:counts=%d", id, ++counts);
	});
	uint32_t id2 = looper->registerTimeEvent(3000, looper, _timerFunction);

	CY_LOG(L_INFO, "begin timer%d and timer%d", id1, id2);
	looper->loop();

	Looper::destroyLooper(looper);

	return 0;
}

