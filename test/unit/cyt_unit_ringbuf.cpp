#include <cy_core.h>
#include <cy_crypt.h>
#include <cy_event.h>
#include <gtest/gtest.h>

using namespace cyclone;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4127)
#endif

namespace {
//-------------------------------------------------------------------------------------
void _fillRandom(uint8_t* mem, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		mem[i] = (uint8_t)(rand() & 0xFF);
	}
}

//-------------------------------------------------------------------------------------
#define CHECK_RINGBUF_EMPTY(rb, c) \
	EXPECT_EQ(0ul, rb.size()); \
	EXPECT_EQ((size_t)(c), rb.capacity()); \
	EXPECT_EQ((size_t)(c), rb.getFreeSize()); \
	EXPECT_TRUE(rb.empty()); \
	EXPECT_FALSE(rb.full());

//-------------------------------------------------------------------------------------
#define CHECK_RINGBUF_SIZE(rb, s, c) \
	EXPECT_EQ((size_t)(s), rb.size()); \
	EXPECT_EQ((size_t)(c), rb.capacity()); \
	EXPECT_EQ((size_t)((c) - (s)), rb.getFreeSize()); \
	if ((s) == 0) \
		EXPECT_TRUE(rb.empty()); \
	else \
		EXPECT_FALSE(rb.empty()); \
	if ((size_t)(s) == rb.capacity()) \
		EXPECT_TRUE(rb.full()); \
	else \
		EXPECT_FALSE(rb.full()); 

//-------------------------------------------------------------------------------------
TEST(RingBuf, Basic)
{
	const char* text_pattern = "Hello,World!";
	const size_t text_length = strlen(text_pattern);

	const size_t buffer_size = RingBuf::kDefaultCapacity * 4;
	const uint8_t* buffer1;
	uint8_t* buffer2;
	{
		uint8_t* _buffer1 = new uint8_t[RingBuf::kDefaultCapacity * 4];
		buffer2 = new uint8_t[RingBuf::kDefaultCapacity * 4];
		_fillRandom(_buffer1, buffer_size);
		_fillRandom(buffer2, buffer_size);
		buffer1 = _buffer1;
	}

	// Initial conditions
	{
		RingBuf rb;
		CHECK_RINGBUF_EMPTY(rb, RingBuf::kDefaultCapacity);
	}

	// Different sizes 
	{
		RingBuf rb(24);
		CHECK_RINGBUF_EMPTY(rb, 24);
	}

	// memcpyInto with zero count
	{
		RingBuf rb;
		rb.memcpyInto(text_pattern, 0);
		CHECK_RINGBUF_EMPTY(rb, RingBuf::kDefaultCapacity);
	}

	// memcpyInto a few bytes of data AND reset
	{
		RingBuf rb;
		rb.memcpyInto(text_pattern, text_length);
		CHECK_RINGBUF_SIZE(rb, text_length, RingBuf::kDefaultCapacity);

		rb.reset();

		CHECK_RINGBUF_EMPTY(rb, RingBuf::kDefaultCapacity);
	}

	// memcpyInto a few bytes twice
	{
		RingBuf rb;
		rb.memcpyInto(text_pattern, text_length);
		rb.memcpyInto(text_pattern, text_length);

		CHECK_RINGBUF_SIZE(rb, text_length * 2, RingBuf::kDefaultCapacity);
	}

	//memcpyInto full capacity AND reset
	{
		RingBuf rb;
		rb.memcpyInto(buffer1, RingBuf::kDefaultCapacity);

		CHECK_RINGBUF_SIZE(rb, RingBuf::kDefaultCapacity, RingBuf::kDefaultCapacity);

		rb.reset();

		CHECK_RINGBUF_EMPTY(rb, RingBuf::kDefaultCapacity);
	}

	//memcpyInto twice to full capacity
	{
		RingBuf rb;
		rb.memcpyInto(buffer1, RingBuf::kDefaultCapacity-2);
		rb.memcpyInto(buffer1 +(RingBuf::kDefaultCapacity - 2), 2);

		CHECK_RINGBUF_SIZE(rb, RingBuf::kDefaultCapacity, RingBuf::kDefaultCapacity);
	}

	//memcpyInto, overflow by 1 byte
	{
		RingBuf rb;
		rb.memcpyInto(buffer1, RingBuf::kDefaultCapacity + 1);

		CHECK_RINGBUF_SIZE(rb, RingBuf::kDefaultCapacity + 1, (RingBuf::kDefaultCapacity + 1) * 2 - 1);
	}

	//memcpyInto twice, overflow by 1 byte on second copy
	{
		RingBuf rb;
		rb.memcpyInto(buffer1, 1);
		rb.memcpyInto(buffer1+1, RingBuf::kDefaultCapacity);

		CHECK_RINGBUF_SIZE(rb, RingBuf::kDefaultCapacity+1, (RingBuf::kDefaultCapacity + 1) * 2 - 1);
	}

	//memcpyOut with zero count
	{
		RingBuf rb;
		rb.memcpyInto(text_pattern, text_length);
		EXPECT_EQ(0ul, rb.memcpyOut(0, 0));

		CHECK_RINGBUF_SIZE(rb, text_length, RingBuf::kDefaultCapacity);
	}

	//memcpyOut a few bytes of data
	{
		RingBuf rb;
		rb.memcpyInto(text_pattern, text_length);

		const size_t READ_SIZE = 8;

		EXPECT_EQ(READ_SIZE, rb.memcpyOut(buffer2, READ_SIZE));

		CHECK_RINGBUF_SIZE(rb, text_length - READ_SIZE, RingBuf::kDefaultCapacity);

		EXPECT_EQ(0, memcmp(buffer2, text_pattern, READ_SIZE));
	}

	_fillRandom(buffer2, buffer_size);

	//memcpyOut all data
	{
		RingBuf rb;
		rb.memcpyInto(text_pattern, text_length);

		const size_t READ_SIZE = text_length;

		EXPECT_EQ(READ_SIZE, rb.memcpyOut(buffer2, READ_SIZE));
		CHECK_RINGBUF_EMPTY(rb, RingBuf::kDefaultCapacity);

		EXPECT_EQ(0, memcmp(buffer2, text_pattern, text_length));
	}

	_fillRandom(buffer2, buffer_size);

	//make wrap condition and memcpyOut
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE*3 < RingBuf::kDefaultCapacity);

		RingBuf rb;
		rb.memcpyInto(buffer1, RingBuf::kDefaultCapacity- TEST_WRAP_SIZE);
		EXPECT_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb.memcpyOut(buffer2, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));
		rb.memcpyInto(buffer1 + RingBuf::kDefaultCapacity- TEST_WRAP_SIZE, TEST_WRAP_SIZE*2);

		EXPECT_EQ(0, memcmp(buffer2, buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));

		CHECK_RINGBUF_SIZE(rb, TEST_WRAP_SIZE * 3, RingBuf::kDefaultCapacity);

		_fillRandom(buffer2, buffer_size);
		EXPECT_EQ(TEST_WRAP_SIZE * 3, rb.memcpyOut(buffer2, TEST_WRAP_SIZE * 3));
		EXPECT_EQ(0, memcmp(buffer2, buffer1+ RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, TEST_WRAP_SIZE * 3));

		CHECK_RINGBUF_EMPTY(rb, RingBuf::kDefaultCapacity);
	}

	_fillRandom(buffer2, buffer_size);

	//make wrap condition and reset
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE * 3 < RingBuf::kDefaultCapacity);

		RingBuf rb;
		rb.memcpyInto(buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		EXPECT_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb.memcpyOut(buffer2, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));
		rb.memcpyInto(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE * 2);

		rb.reset();
		CHECK_RINGBUF_EMPTY(rb, RingBuf::kDefaultCapacity);
	}

	_fillRandom(buffer2, buffer_size);

	//make wrap condition and overflow
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE*3 < RingBuf::kDefaultCapacity);

		RingBuf rb;
		rb.memcpyInto(buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		EXPECT_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb.memcpyOut(buffer2, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));
		rb.memcpyInto(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE * 2);

		//overflow
		rb.memcpyInto(buffer1 + RingBuf::kDefaultCapacity + TEST_WRAP_SIZE, RingBuf::kDefaultCapacity);

		CHECK_RINGBUF_SIZE(rb, RingBuf::kDefaultCapacity + TEST_WRAP_SIZE * 3, (RingBuf::kDefaultCapacity + 1) * 2 - 1);

		_fillRandom(buffer2, buffer_size);
		EXPECT_EQ(RingBuf::kDefaultCapacity + TEST_WRAP_SIZE * 3, rb.memcpyOut(buffer2, rb.size()));
		EXPECT_EQ(0, memcmp(buffer2, buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb.size()));
	}

	_fillRandom(buffer2, buffer_size);

	//copyto
	{
		const size_t COPY_SIZE = 8;

		RingBuf rb1, rb2;
		rb1.memcpyInto(text_pattern, text_length);
		EXPECT_EQ(COPY_SIZE, rb1.copyto(&rb2, COPY_SIZE));

		CHECK_RINGBUF_SIZE(rb1, text_length- COPY_SIZE, RingBuf::kDefaultCapacity);
		CHECK_RINGBUF_SIZE(rb2, COPY_SIZE, RingBuf::kDefaultCapacity);

		_fillRandom(buffer2, buffer_size);

		EXPECT_EQ(COPY_SIZE, rb2.memcpyOut(buffer2, COPY_SIZE));
		EXPECT_EQ(0, memcmp(buffer2, text_pattern, COPY_SIZE));
	}

	_fillRandom(buffer2, buffer_size);

	//make wrap condition and copyto
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE * 4 < RingBuf::kDefaultCapacity);
		const size_t COPY_SIZE = TEST_WRAP_SIZE*3;

		RingBuf rb1, rb2;
		rb1.memcpyInto(buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		EXPECT_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb1.memcpyOut(buffer2, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));
		rb1.memcpyInto(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE * 3);

		EXPECT_EQ(COPY_SIZE, rb1.copyto(&rb2, COPY_SIZE));

		CHECK_RINGBUF_SIZE(rb1, TEST_WRAP_SIZE, RingBuf::kDefaultCapacity);
		CHECK_RINGBUF_SIZE(rb2, COPY_SIZE, RingBuf::kDefaultCapacity);

		_fillRandom(buffer2, buffer_size);

		EXPECT_EQ(COPY_SIZE, rb2.memcpyOut(buffer2, COPY_SIZE));
		EXPECT_EQ(0, memcmp(buffer2, buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb2.size()));
	}

	_fillRandom(buffer2, buffer_size);

	//peek
	{
		const size_t PEEK_SIZE = 8;

		RingBuf rb1;
		rb1.memcpyInto(text_pattern, text_length);

		EXPECT_EQ(PEEK_SIZE, rb1.peek(0, buffer2, PEEK_SIZE));
		CHECK_RINGBUF_SIZE(rb1, text_length, RingBuf::kDefaultCapacity);
		EXPECT_EQ(0, memcmp(buffer2, text_pattern, PEEK_SIZE));

		_fillRandom(buffer2, buffer_size);
		EXPECT_EQ(PEEK_SIZE, rb1.peek(1, buffer2, PEEK_SIZE));

		CHECK_RINGBUF_SIZE(rb1, text_length, RingBuf::kDefaultCapacity);
		EXPECT_EQ(0, memcmp(buffer2, text_pattern+1, PEEK_SIZE));
	}

	_fillRandom(buffer2, buffer_size);

	//make wrap condition and peek
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE * 4 < RingBuf::kDefaultCapacity);

		RingBuf rb1;
		rb1.memcpyInto(buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		EXPECT_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb1.memcpyOut(buffer2, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));
		rb1.memcpyInto(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE * 3);

		_fillRandom(buffer2, buffer_size);
		EXPECT_EQ(TEST_WRAP_SIZE, rb1.peek(0, buffer2, TEST_WRAP_SIZE));
		CHECK_RINGBUF_SIZE(rb1, TEST_WRAP_SIZE * 4, RingBuf::kDefaultCapacity);
		EXPECT_EQ(0, memcmp(buffer2, buffer1+ RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, TEST_WRAP_SIZE));

		_fillRandom(buffer2, buffer_size);
		EXPECT_EQ(TEST_WRAP_SIZE * 2, rb1.peek(TEST_WRAP_SIZE, buffer2, TEST_WRAP_SIZE*2));
		CHECK_RINGBUF_SIZE(rb1, TEST_WRAP_SIZE * 4, RingBuf::kDefaultCapacity);
		EXPECT_EQ(0, memcmp(buffer2, buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE*2));

		_fillRandom(buffer2, buffer_size);
		EXPECT_EQ(TEST_WRAP_SIZE, rb1.peek(TEST_WRAP_SIZE*3, buffer2, TEST_WRAP_SIZE));
		CHECK_RINGBUF_SIZE(rb1, TEST_WRAP_SIZE * 4, RingBuf::kDefaultCapacity);
		EXPECT_EQ(0, memcmp(buffer2, buffer1 + RingBuf::kDefaultCapacity + TEST_WRAP_SIZE, TEST_WRAP_SIZE));
	}

	_fillRandom(buffer2, buffer_size);

	//discard
	{
		const size_t DISCARD_SIZE = 8;

		RingBuf rb1;
		rb1.memcpyInto(text_pattern, text_length);

		EXPECT_EQ(DISCARD_SIZE, rb1.discard(DISCARD_SIZE));
		CHECK_RINGBUF_SIZE(rb1, text_length-DISCARD_SIZE, RingBuf::kDefaultCapacity);

		EXPECT_EQ(text_length - DISCARD_SIZE, rb1.memcpyOut(buffer2, text_length));
		EXPECT_EQ(0, memcmp(buffer2, text_pattern+ DISCARD_SIZE, text_length - DISCARD_SIZE));

		CHECK_RINGBUF_EMPTY(rb1, RingBuf::kDefaultCapacity);
		rb1.memcpyInto(text_pattern, text_length);
		EXPECT_EQ(text_length, rb1.discard(RingBuf::kDefaultCapacity));
		CHECK_RINGBUF_EMPTY(rb1, RingBuf::kDefaultCapacity);
	}

	_fillRandom(buffer2, buffer_size);

	//make wrap condition and discard
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE * 4 < RingBuf::kDefaultCapacity);

		RingBuf rb1;
		rb1.memcpyInto(buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		EXPECT_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb1.memcpyOut(buffer2, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));
		rb1.memcpyInto(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE * 3);

		EXPECT_EQ(TEST_WRAP_SIZE * 3, rb1.discard(TEST_WRAP_SIZE * 3));
		CHECK_RINGBUF_SIZE(rb1, TEST_WRAP_SIZE, RingBuf::kDefaultCapacity);

		_fillRandom(buffer2, buffer_size);
		EXPECT_EQ(TEST_WRAP_SIZE, rb1.memcpyOut(buffer2, TEST_WRAP_SIZE));
		EXPECT_EQ(0, memcmp(buffer2, buffer1 + RingBuf::kDefaultCapacity + TEST_WRAP_SIZE, TEST_WRAP_SIZE));
	}

	//checksum
	{
		RingBuf rb1;
		rb1.memcpyInto(text_pattern, text_length);

		EXPECT_EQ(INITIAL_ADLER, rb1.checksum(text_length, 0));
		EXPECT_EQ(INITIAL_ADLER, rb1.checksum(text_length, 1));
		EXPECT_EQ(INITIAL_ADLER, rb1.checksum(0, text_length+1));
		EXPECT_EQ(INITIAL_ADLER, rb1.checksum(0,0));

		EXPECT_EQ(0x1c9d044aul, rb1.checksum(0, text_length));
		EXPECT_EQ(0x0d0c02e7ul, rb1.checksum(0, 8));
		EXPECT_EQ(0x0ddc0311ul, rb1.checksum(1, 8));

		CHECK_RINGBUF_SIZE(rb1, text_length, RingBuf::kDefaultCapacity);
	}

	//make wrap condition and checksum
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE * 4 < RingBuf::kDefaultCapacity);

		RingBuf rb1;
		rb1.memcpyInto(buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		EXPECT_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb1.memcpyOut(buffer2, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));
		rb1.memcpyInto(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE * 3);

		EXPECT_EQ(adler32(INITIAL_ADLER, buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, TEST_WRAP_SIZE), rb1.checksum(0, TEST_WRAP_SIZE));
		EXPECT_EQ(adler32(INITIAL_ADLER, buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, TEST_WRAP_SIZE*3), rb1.checksum(0, TEST_WRAP_SIZE*3));
		EXPECT_EQ(adler32(INITIAL_ADLER, buffer1 + RingBuf::kDefaultCapacity, TEST_WRAP_SIZE), rb1.checksum(TEST_WRAP_SIZE*2, TEST_WRAP_SIZE));
		EXPECT_EQ(adler32(INITIAL_ADLER, buffer1 + RingBuf::kDefaultCapacity+ TEST_WRAP_SIZE, TEST_WRAP_SIZE), rb1.checksum(TEST_WRAP_SIZE * 3, TEST_WRAP_SIZE));
	}

	//normalize
	{
		RingBuf rb1;
		rb1.memcpyInto(text_pattern, text_length);

		EXPECT_EQ(0, memcmp(text_pattern, rb1.normalize(), text_length));
		CHECK_RINGBUF_SIZE(rb1, text_length, RingBuf::kDefaultCapacity);

		rb1.discard(1);
		EXPECT_EQ(0, memcmp(text_pattern+1, rb1.normalize(), text_length-1));
		CHECK_RINGBUF_SIZE(rb1, text_length-1, RingBuf::kDefaultCapacity);
	}

	//make wrap condition and normalize
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE * 4 < RingBuf::kDefaultCapacity);

		RingBuf rb1;
		rb1.memcpyInto(buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		EXPECT_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, rb1.discard(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2));
		rb1.memcpyInto(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE * 2);
		EXPECT_EQ(0, memcmp(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE*2, rb1.normalize(), TEST_WRAP_SIZE*3));
		CHECK_RINGBUF_SIZE(rb1, TEST_WRAP_SIZE * 3, RingBuf::kDefaultCapacity);

		rb1.reset();
		rb1.memcpyInto(buffer1, RingBuf::kDefaultCapacity);
		EXPECT_EQ(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, rb1.discard(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE));
		rb1.memcpyInto(buffer1 + RingBuf::kDefaultCapacity, TEST_WRAP_SIZE * 2);
		EXPECT_EQ(0, memcmp(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, rb1.normalize(), TEST_WRAP_SIZE * 3));
		CHECK_RINGBUF_SIZE(rb1, TEST_WRAP_SIZE * 3, RingBuf::kDefaultCapacity);
	}
}


//-------------------------------------------------------------------------------------
TEST(RingBuf, Socket)
{
	const char* text_pattern = "Hello,World!";
	const size_t text_length = strlen(text_pattern);

	const size_t buffer_size = RingBuf::kDefaultCapacity * 4;
	const uint8_t* buffer1;
	uint8_t* buffer2;
	{
		uint8_t* _buffer1 = new uint8_t[RingBuf::kDefaultCapacity * 4];
		buffer2 = new uint8_t[RingBuf::kDefaultCapacity * 4];
		_fillRandom(_buffer1, buffer_size);
		_fillRandom(buffer2, buffer_size);
		buffer1 = _buffer1;
	}

	//writeSocket/readSocket
	{
		RingBuf rb_snd;
		Pipe pipe;

		//send few bytes
		rb_snd.memcpyInto(text_pattern, text_length);
		EXPECT_EQ(text_length, (size_t)rb_snd.writeSocket(pipe.get_write_port()));
		CHECK_RINGBUF_EMPTY(rb_snd, RingBuf::kDefaultCapacity);

		RingBuf rb_rcv;
		EXPECT_EQ(text_length, (size_t)rb_rcv.readSocket(pipe.get_read_port()));
		CHECK_RINGBUF_SIZE(rb_rcv, text_length, RingBuf::kDefaultCapacity);
		EXPECT_EQ(0, memcmp(rb_rcv.normalize(), text_pattern, text_length));
		rb_rcv.discard(text_length);
		CHECK_RINGBUF_EMPTY(rb_rcv, RingBuf::kDefaultCapacity);

		uint32_t read_buf;
		EXPECT_EQ(SOCKET_ERROR, pipe.read((char*)&read_buf, sizeof(read_buf)));
		EXPECT_TRUE(socket_api::isLastErrorWOULDBLOCK());

		//send and receive again
		rb_snd.memcpyInto(text_pattern, text_length);
		rb_snd.memcpyInto(text_pattern, text_length);
		rb_snd.discard(text_length);
		CHECK_RINGBUF_SIZE(rb_snd, text_length, RingBuf::kDefaultCapacity);

		rb_rcv.memcpyInto(text_pattern, text_length);
		rb_rcv.memcpyInto(text_pattern, text_length);
		rb_rcv.discard(text_length);
		CHECK_RINGBUF_SIZE(rb_rcv, text_length, RingBuf::kDefaultCapacity);

		EXPECT_EQ(text_length, (size_t)rb_snd.writeSocket(pipe.get_write_port()));
		EXPECT_EQ(text_length, (size_t)rb_rcv.readSocket(pipe.get_read_port()));
		CHECK_RINGBUF_EMPTY(rb_snd, RingBuf::kDefaultCapacity);
		CHECK_RINGBUF_SIZE(rb_rcv, text_length*2, RingBuf::kDefaultCapacity);

		EXPECT_EQ(0, memcmp(rb_rcv.normalize(), text_pattern, text_length));
		EXPECT_EQ(0, memcmp(rb_rcv.normalize()+text_length, text_pattern, text_length));
	}

	//readSocket to full
	{
		Pipe pipe;

		RingBuf rb_rcv;
		rb_rcv.memcpyInto(text_pattern, text_length);
		rb_rcv.memcpyInto(text_pattern, text_length);
		rb_rcv.discard(text_length);
		CHECK_RINGBUF_SIZE(rb_rcv, text_length, RingBuf::kDefaultCapacity);

		EXPECT_EQ(RingBuf::kDefaultCapacity, pipe.write((const char*)buffer1, RingBuf::kDefaultCapacity));

		EXPECT_EQ(RingBuf::kDefaultCapacity-text_length, (size_t)rb_rcv.readSocket(pipe.get_read_port(), false));
		CHECK_RINGBUF_SIZE(rb_rcv, RingBuf::kDefaultCapacity, RingBuf::kDefaultCapacity);

		EXPECT_EQ(0, memcmp(rb_rcv.normalize(), text_pattern, text_length));
		EXPECT_EQ(0, memcmp(rb_rcv.normalize() + text_length, buffer1, RingBuf::kDefaultCapacity-text_length));
	}

	//readSocket and expand
	{
		Pipe pipe;

		RingBuf rb_rcv;
		rb_rcv.memcpyInto(text_pattern, text_length);
		rb_rcv.memcpyInto(text_pattern, text_length);
		rb_rcv.discard(text_length);
		CHECK_RINGBUF_SIZE(rb_rcv, text_length, RingBuf::kDefaultCapacity);

		EXPECT_EQ(RingBuf::kDefaultCapacity, pipe.write((const char*)buffer1, RingBuf::kDefaultCapacity));

		EXPECT_EQ(RingBuf::kDefaultCapacity, rb_rcv.readSocket(pipe.get_read_port()));
		CHECK_RINGBUF_SIZE(rb_rcv, text_length + RingBuf::kDefaultCapacity, (RingBuf::kDefaultCapacity + 1) * 2 - 1);

		EXPECT_EQ(0, memcmp(rb_rcv.normalize(), text_pattern, text_length));
		EXPECT_EQ(0, memcmp(rb_rcv.normalize() + text_length, buffer1, RingBuf::kDefaultCapacity));
	}

	//make wrap condition and writeSocket
	{
		const size_t TEST_WRAP_SIZE = 32;
		assert(TEST_WRAP_SIZE * 3 < RingBuf::kDefaultCapacity);

		RingBuf rb_snd;
		rb_snd.memcpyInto(buffer1, RingBuf::kDefaultCapacity - TEST_WRAP_SIZE);
		rb_snd.discard(RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2);
		rb_snd.memcpyInto(buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE, TEST_WRAP_SIZE * 3);
		CHECK_RINGBUF_SIZE(rb_snd, TEST_WRAP_SIZE * 4, RingBuf::kDefaultCapacity);

		Pipe pipe;
		EXPECT_EQ(TEST_WRAP_SIZE * 4, (size_t)rb_snd.writeSocket(pipe.get_write_port()));
		CHECK_RINGBUF_EMPTY(rb_snd, RingBuf::kDefaultCapacity);

		RingBuf rb_rcv;
		EXPECT_EQ(TEST_WRAP_SIZE * 4, (size_t)rb_rcv.readSocket(pipe.get_read_port()));
		CHECK_RINGBUF_SIZE(rb_rcv, TEST_WRAP_SIZE * 4, RingBuf::kDefaultCapacity);

		EXPECT_EQ(0, memcmp(rb_rcv.normalize(), buffer1 + RingBuf::kDefaultCapacity - TEST_WRAP_SIZE * 2, TEST_WRAP_SIZE * 4));
	}
}

}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

