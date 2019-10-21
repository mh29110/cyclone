#include <cy_event.h>
#include "cyt_event_fortest.h"

#include <gtest/gtest.h>

using namespace cyclone;

namespace {

//-------------------------------------------------------------------------------------
TEST(EventLooper, Basic)
{
	EventLooper_ForTest looper;
	const size_t default_channel_counts = EventLooper_ForTest::get_DEFAULT_CHANNEL_BUF_COUNTS();

	const auto& channels = looper.get_channel_buf();
	CHECK_CHANNEL_SIZE(0, 0, 0);

	Looper::event_id_t id = looper.registerTimeEvent(1, 0, 0);
	CHECK_CHANNEL_SIZE(default_channel_counts, 1, default_channel_counts - 1);

	looper.disableAll(id);
	CHECK_CHANNEL_SIZE(default_channel_counts, 0, default_channel_counts - 1);

	looper.enableRead(id);
	CHECK_CHANNEL_SIZE(default_channel_counts, 1, default_channel_counts - 1);

	looper.disableAll(id);
	CHECK_CHANNEL_SIZE(default_channel_counts, 0, default_channel_counts - 1);

	looper.deleteEvent(id);
	CHECK_CHANNEL_SIZE(default_channel_counts, 0, default_channel_counts);

	std::vector<Looper::event_id_t> id_buffer;
	for (size_t i = 0; i < default_channel_counts; i++) {
		id_buffer.push_back(looper.registerTimeEvent(1, 0, 0));
		CHECK_CHANNEL_SIZE(default_channel_counts, i+1, default_channel_counts - i - 1);
	}

	id_buffer.push_back(looper.registerTimeEvent(1, 0, 0));
	CHECK_CHANNEL_SIZE(default_channel_counts * 2, default_channel_counts + 1, default_channel_counts - 1);

	looper.disableAll(id_buffer[id_buffer.size()-1]);
	CHECK_CHANNEL_SIZE(default_channel_counts * 2, default_channel_counts, default_channel_counts - 1);

	looper.deleteEvent(id_buffer[id_buffer.size() - 1]);
	CHECK_CHANNEL_SIZE(default_channel_counts * 2, default_channel_counts, default_channel_counts);

	for (size_t i = 0; i < default_channel_counts; i++) {
		looper.disableAll(id_buffer[i]);
		CHECK_CHANNEL_SIZE(default_channel_counts * 2, default_channel_counts-i-1, default_channel_counts+i);

		looper.deleteEvent(id_buffer[i]);
		CHECK_CHANNEL_SIZE(default_channel_counts * 2, default_channel_counts-i-1, default_channel_counts+i+1);
	}

	CHECK_CHANNEL_SIZE(default_channel_counts * 2, 0, default_channel_counts*2);
}

}
