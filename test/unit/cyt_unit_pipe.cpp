#include <cy_core.h>
#include <cy_crypt.h>
#include <cy_event.h>
#include <gtest/gtest.h>

using namespace cyclone;

namespace {
//-------------------------------------------------------------------------------------
TEST(Pipe, Basic)
{
	Pipe pipe;

	const char* plain_text = "Hello,World!";
	const size_t text_len = strlen(plain_text);

	const size_t readbuf_len = 1024;
	char read_buf[readbuf_len] = { 0 };

	ssize_t read_size = pipe.read(read_buf, readbuf_len);
	EXPECT_TRUE(socket_api::isLastErrorWOULDBLOCK());
	EXPECT_EQ(SOCKET_ERROR, read_size);

	ssize_t write_size = pipe.write(plain_text, text_len);
	EXPECT_EQ(text_len, (size_t)write_size);

	read_size = pipe.read(read_buf, readbuf_len);
	EXPECT_EQ(text_len, (size_t)read_size);
	EXPECT_STREQ(plain_text, read_buf);

	read_size = pipe.read(read_buf, readbuf_len);
	EXPECT_TRUE(socket_api::isLastErrorWOULDBLOCK());
	EXPECT_EQ(SOCKET_ERROR, read_size);
}

//-------------------------------------------------------------------------------------
TEST(Pipe, Overflow)
{
	Pipe pipe;

	XorShift128 rndPush, rndPop;
	rndPush.make();
	rndPop.seed0 = rndPush.seed0;
	rndPop.seed1 = rndPush.seed1;

	const size_t snd_block_size = 1024;
	assert(snd_block_size % sizeof(uint64_t) == 0);

	char snd_block[snd_block_size] = { 0 };
	size_t rnd_counts = snd_block_size / sizeof(uint64_t);

	size_t total_snd_size = 0;
	for (;;) {
		for (size_t i = 0; i < rnd_counts; i++) {
			uint64_t rnd = rndPush.next();
			((uint64_t*)snd_block)[i] = rnd;
		}

		ssize_t write_size = pipe.write(snd_block, snd_block_size);
		if (write_size <= 0) {
			EXPECT_TRUE(socket_api::isLastErrorWOULDBLOCK());
			break;
		}
		total_snd_size += (size_t)write_size;
	}

	size_t total_rcv_size = 0;
	for (;;) {
		uint64_t read_data;
		ssize_t read_size = pipe.read((char*)&read_data, sizeof(read_data));
		if (read_size <= 0) {
			EXPECT_TRUE(socket_api::isLastErrorWOULDBLOCK());
			break;
		}
		total_rcv_size += (size_t)read_size;
		EXPECT_EQ(read_data, rndPop.next());
	}

	EXPECT_EQ(total_snd_size, total_rcv_size);
}

//-------------------------------------------------------------------------------------
struct ThreadData
{
	Pipe pipe;
	XorShift128 rnd;
	size_t total_size;
};

//-------------------------------------------------------------------------------------
void _push_function(void* param)
{
	ThreadData* data = (ThreadData*)param;
	XorShift128 rnd = data->rnd;
	RingBuf sndBuf;

	size_t total_snd_size = 0;

	for (;;) {
		while (sndBuf.getFreeSize() >= sizeof(uint64_t) && total_snd_size<(data->total_size)) {
			uint64_t next_snd = rnd.next();
			sndBuf.memcpyInto(&next_snd, sizeof(next_snd));
			total_snd_size += sizeof(next_snd);
		}

		if (sndBuf.empty()) break;

		ssize_t write_size = sndBuf.writeSocket(data->pipe.get_write_port());
		if (write_size <= 0) {
			sys_api::threadYield();
			continue;
		}
	}
	EXPECT_EQ(total_snd_size, data->total_size);
}

//-------------------------------------------------------------------------------------
void _pop_function(void* param)
{
	ThreadData* data = (ThreadData*)param;
	XorShift128 rnd = data->rnd;
	RingBuf rcvBuf;

	size_t total_rcv_size = 0;
	uint64_t read_data;

	while (total_rcv_size<(data->total_size)) {
		ssize_t read_size = rcvBuf.readSocket(data->pipe.get_read_port(), false);
		while (rcvBuf.size() >= sizeof(uint64_t)) {
			EXPECT_EQ(sizeof(uint64_t), rcvBuf.memcpyOut(&read_data, sizeof(uint64_t)));
			EXPECT_EQ(rnd.next(), read_data);
			total_rcv_size += sizeof(uint64_t);
		}

		if (read_size <= 0) {
			sys_api::threadYield();
		}
	}
	EXPECT_TRUE(rcvBuf.empty());
	EXPECT_EQ(total_rcv_size, data->total_size);
	EXPECT_EQ(SOCKET_ERROR, (data->pipe).read((char*)&read_data, sizeof(read_data)));
	EXPECT_TRUE(socket_api::isLastErrorWOULDBLOCK());
	EXPECT_EQ(RingBuf::kDefaultCapacity, rcvBuf.capacity());
}

//-------------------------------------------------------------------------------------
TEST(Pipe, MultiThread)
{
	ThreadData data;
	data.rnd.make();
	data.total_size = 1024 * 1024*10; //1MB

	thread_t pop_thread = sys_api::threadCreate(_pop_function, &data, "pop");
	thread_t push_thread = sys_api::threadCreate(_push_function, &data, "push");

	//join
	sys_api::threadJoin(pop_thread);
	sys_api::threadJoin(push_thread);
}

}