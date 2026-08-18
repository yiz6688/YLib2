#include"RingBuffer.h"
#include<algorithm>
#include<bit>
#include<limits>
#include<cassert>

RingBuffer::RingBuffer(unsigned bufferSize)
	:_capacity{ std::bit_ceil(bufferSize) }, _mask{ _capacity - 1 }, _gap{ 1 }, 
	_buffer(_mask + 1), _ptr{ _buffer.data() }
{
	this->_cap_aligned = (this->_capacity / this->_gap) * this->_gap;
	this->_max_size = this->_cap_aligned - this->_gap;
}

RingBuffer::RingBuffer(char* buffer, unsigned size)
	:_capacity{ size }, _mask{ size - 1 }, _gap{ 1 }, _buffer(), _ptr{ buffer }
{
	assert(size>0 &&(size & (size - 1)) == 0); // size must be a power of 2
	assert(buffer != nullptr);

	this->_cap_aligned = (this->_capacity / this->_gap) * this->_gap;
	this->_max_size = this->_cap_aligned - this->_gap;

}

RingBuffer::~RingBuffer()
{
	this->_ptr = nullptr;
}

RingBuffer::RingBuffer(RingBuffer&& value) noexcept
{
	this->_capacity = value._capacity;
	this->_mask = value._mask;
	this->_gap = value._gap;
	this->_cap_aligned = value._cap_aligned;
	this->_max_size = value._max_size;
	this->_buffer = std::move(value._buffer);
	this->_ptr = value._ptr;
	this->write_pos.store(value.write_pos.load());
	this->read_pos.store(value.read_pos.load());
	
	this->write_lock_len = value.write_lock_len;
	this->read_lock_len = value.read_lock_len;
}

RingBuffer& RingBuffer::operator=(RingBuffer&& value) noexcept
{
	if (&value == this)
	{
		return *this;
	}
	this->_capacity = value._capacity;
	this->_mask = value._mask;
	this->_gap = value._gap;
	this->_cap_aligned = value._cap_aligned;
	this->_max_size = value._max_size;
	this->_buffer = std::move(value._buffer);
	this->_ptr = value._ptr;
	this->write_pos.store(value.write_pos.load());
	this->read_pos.store(value.read_pos.load());
	this->write_lock_len = value.write_lock_len;
	this->read_lock_len = value.read_lock_len;
	return *this;
}

int RingBuffer::read(char* buffer, int size)
{
	return this->read(buffer, 0, size);
}

int RingBuffer::read(char* buffer, int offset, int size)
{

	if(this->read_lock_len != 0 || size == 0)
	{
		return 0;
	}

	auto wpos = this->write_pos.load(std::memory_order_acquire);
	auto rpos = this->read_pos.load(std::memory_order_relaxed);

	

	int readPos = rpos & this->_mask;

	auto readableBytes = this->calcReadableBytes(wpos, rpos); //计算可读空间
	if(readableBytes == 0)
	{
		return 0;
	}
	if (readableBytes > size)
	{
		readableBytes = size;  //修正可读数量
	}


	auto size1 = this->_cap_aligned - readPos;  //读指针到尾部的空间
	if (size1 > readableBytes)
	{
		size1 = readableBytes;
	}
	std::copy_n(this->_ptr + readPos, size1, buffer + offset);

	auto size2 = readableBytes - size1;
	if (size2 > 0)
	{
		offset += size1;
		std::copy_n(this->_ptr, size2, buffer + offset);
	}

	rpos = this->calcPos(rpos, readableBytes);
	this->read_pos.store(rpos, std::memory_order_release);//或者更新已用空间容量。


	return readableBytes;
}



int RingBuffer::write(const char* data, int size)
{
	return this->write(data, 0, size);
}

/**
*往环形缓冲区写数据，缓冲区满了以后进行覆盖。
*/
int RingBuffer::write(const char* data, int offset, int size)
{
	if(this->write_lock_len != 0 || size == 0)
	{
		return 0;
	}

	auto wpos = this->write_pos.load(std::memory_order_relaxed);
	auto rpos = this->read_pos.load(std::memory_order_acquire);
	

	auto writePos = wpos & this->_mask;

	auto writeableBytes = this->calcWriteableBytes(wpos, rpos); //计算可写空间
	if(writeableBytes == 0)
	{
		return 0;
	}

	if (writeableBytes > size)
	{
		writeableBytes = size;
	}

	auto size1 = this->_cap_aligned - writePos;  //读指针到尾部的空间
	if (size1 > writeableBytes)
	{
		size1 = writeableBytes;
	}
	std::copy_n(data + offset, size1, this->_ptr + writePos);

	auto size2 = writeableBytes - size1;
	if (size2 > 0)
	{
		offset += size1;
		std::copy_n(data + offset, size2, this->_ptr); //从头部开始写
	}

	wpos = this->calcPos(wpos, writeableBytes); //修正写指针
	this->write_pos.store(wpos, std::memory_order::release);

	return writeableBytes;
}

int RingBuffer::readFrom(RingBuffer& ring, int size)
{
	int nCapacity = ring.getCapacity();
	//int nReadPos = 	ring.getReadPos();

	int nReadPos = 0;// 待验证修复
	int nReadSize = ring.getReadableBytes();
	int nEndPos = nReadPos + nReadSize;

	int size1 = nCapacity - nReadPos;
	int size2 = 0;

	int ReadSize = 0;

	if (size1 > nReadPos)
	{
		size1 = nReadPos;
	}
	else
	{
		size2 = nReadPos - size1;
	}
	char* ptr1 = ring._ptr + nReadPos;
	ReadSize = this->write(ptr1, size1);
	
	if (size2 > 0)
	{
		size1 = this->write(ring._ptr, size2);
		ReadSize += size1;
	}


	return ReadSize;
}

int RingBuffer::writeTo(RingBuffer& ring, int size)
{
	return ring.readFrom(*this, size);
}

std::span<char> RingBuffer::getWriteBuffer(unsigned size)
{
	auto wpos = this->write_pos.load(std::memory_order_relaxed);
	auto writePos = wpos & this->_mask;  //相当于求余
	auto wptr = this->_ptr + writePos;  //写指针起始位置。

	if(size == 0 || this->write_lock_len != 0)
	{
		return std::span<char>(wptr, 0);
	}

	auto rpos = this->read_pos.load(std::memory_order_acquire);

	auto writeableBytes = this->calcWriteableBytes(wpos, rpos); //计算剩余可写空间
	if(writeableBytes > size)
	{
		writeableBytes = size;
	}

	auto tailBytes = this->_cap_aligned - wpos;  //写指针到尾部的空间
	if(tailBytes > writeableBytes) //说明没有回绕
	{
		this->write_lock_len = writeableBytes;
	}else
	{
		this->write_lock_len = tailBytes;
	}

	this->write_pos.store(wpos, std::memory_order_release);
    return std::span<char>(wptr, this->write_lock_len);
}

int RingBuffer::releaseWriteBuffer(TYPE1 size)
{
	if(size == 0 || this->write_lock_len == 0)
	{
		return 0;
	}else if(size == std::numeric_limits<TYPE1>::max())
	{
		size = this->write_lock_len;
	}

	if(size > this->write_lock_len)
	{
		size = this->write_lock_len;
	}

	//这里必定不会产生回绕
	auto wpos = this->write_pos.load(std::memory_order_relaxed);

	auto rpos = this->read_pos.load(std::memory_order_acquire);

	auto releaseSize = this->write_lock_len > size ? size : this->write_lock_len;
	wpos = this->calcPos(wpos, releaseSize);
	this->write_lock_len -= releaseSize;

	this->write_pos.store(wpos, std::memory_order_release);
    return releaseSize;
}

int RingBuffer::releaseWriteBuffer()
{
   return this->releaseWriteBuffer(-1);
}

std::span<char> RingBuffer::getReadBuffer(TYPE1 size)
{
	auto rpos = this->read_pos.load(std::memory_order_relaxed);

	auto readPos = rpos & this->_mask;  //相当于求余
	auto rptr = this->_ptr + readPos;  //写指针起始位置。

	if(size == 0 || this->read_lock_len == 0)
	{
		return std::span<char>(rptr, 0);
	}
	
	auto wpos = this->write_pos.load(std::memory_order_acquire);

	auto readableBytes = this->calcReadableBytes(wpos, rpos); //计算可读空间
	if(readableBytes > size)
	{
		readableBytes = size;
	}

	auto tailBytes = this->_cap_aligned - readPos;  //写指针到尾部的空间
	if(tailBytes > readableBytes) //说明没有回绕
	{
		this->read_lock_len = readableBytes;
	}else
	{
		this->read_lock_len = tailBytes;
	}

	this->read_pos.store(rpos, std::memory_order_release);
    return std::span<char>(rptr, this->read_lock_len);
}

int RingBuffer::releaseReadBuffer(unsigned size)
{
    if(size == 0 || this->read_lock_len == 0)
	{
		return 0;
	}else if(this->read_lock_len == std::numeric_limits<TYPE1>::max())
	{
		size = this->read_lock_len;
	}
	//这里必定不会产生回绕
	auto wpos = this->write_pos.load(std::memory_order_acquire);
	auto rpos = this->read_pos.load(std::memory_order_relaxed);

	auto releaseSize = this->read_lock_len > size ? size : this->read_lock_len;
	rpos = this->calcPos(rpos , releaseSize);
	this->read_lock_len -= releaseSize;

	this->read_pos.store(rpos, std::memory_order_release);
    return releaseSize;
}

int RingBuffer::releaseReadBuffer()
{
	return this->releaseReadBuffer(-1);
}
