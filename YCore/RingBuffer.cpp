#include"RingBuffer.h"
#include<algorithm>
#include<bit>
#include<cassert>

RingBuffer::RingBuffer(unsigned bufferSize)
	:_capacity{ std::bit_ceil(bufferSize) - 1 }, _buffer(_capacity), _ptr{ _buffer.data() }
{
	
}

RingBuffer::RingBuffer(char* buffer, unsigned size)
	:_capacity{ size - 1 }, _buffer(), _ptr{ buffer }
{
	assert(size>0 &&(size & (size - 1)) == 0); // size must be a power of 2
	assert(buffer != nullptr);
}

RingBuffer::~RingBuffer()
{
	this->_ptr = nullptr;
}

RingBuffer::RingBuffer(RingBuffer&& value) noexcept
{
	this->_capacity = value._capacity;
	this->_buffer = std::move(value._buffer);
	this->_ptr = value._ptr;
	this->write_param.store(value.write_param.load());
	this->read_param.store(value.read_param.load());
}

RingBuffer& RingBuffer::operator=(RingBuffer&& value) noexcept
{
	if (&value == this)
	{
		return *this;
	}
	this->_capacity = value._capacity;
	this->_buffer = std::move(value._buffer);
	this->_ptr = value._ptr;
	this->write_param.store(value.write_param.load());
	this->read_param.store(value.read_param.load());
	return *this;
}

int RingBuffer::read(char* buffer, int size)
{
	return this->read(buffer, 0, size);
}

int RingBuffer::read(char* buffer, int offset, int size)
{
	auto wparam = this->write_param.load(std::memory_order_acquire);
	auto rparam = this->read_param.load(std::memory_order_acquire);

	if(rparam.len != 0)
	{
		return 0;
	}

	int readPos = rparam.pos & this->_capacity;
	if (readPos < 0)
	{
		readPos += this->_capacity;
	}

	auto readBytes = wparam.pos - rparam.pos;
	if (readBytes > size)
	{
		readBytes = size;  //修正可读数量
	}


	auto size1 = this->_capacity - readPos;  //读指针到尾部的空间
	if (size1 > readBytes)
	{
		size1 = readBytes;
	}
	std::copy_n(this->_ptr + readPos, size1, buffer + offset);

	auto size2 = readBytes - size1;
	if (size2 > 0)
	{
		offset += size1;
		std::copy_n(this->_ptr, size2, buffer + offset);
	}

	rparam.pos += readBytes;
	this->read_param.store(rparam, std::memory_order_release);//或者更新已用空间容量。


	return readBytes;
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
	auto wparam = this->write_param.load(std::memory_order_acquire);
	auto rparam = this->read_param.load(std::memory_order_acquire);
	if(wparam.len != 0)
	{
		return 0;
	}

	auto writePos = wparam.pos & this->_capacity;
	//auto remaindBytes = this->_capacity - usedCount;  //剩余容量
	auto writeBytes = size;
	if (writeBytes > this->_capacity)
	{
		writeBytes = this->_capacity;
	}

	auto size1 = this->_capacity - writePos;  //读指针到尾部的空间
	if (size1 > writeBytes)
	{
		size1 = writeBytes;
	}
	std::copy_n(data + offset, size1, this->_ptr + writePos);

	auto size2 = writeBytes - size1;
	if (size2 > 0)
	{
		offset += size1;
		std::copy_n(data + offset, size2, this->_ptr); //从头部开始写
	}

	wparam.pos += writeBytes;
	writePos += writeBytes;
	if (writePos > this->_capacity)
	{
		writePos -= this->_capacity;
	}

	this->write_param.store(wparam, std::memory_order::release);

	return writeBytes;
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
	auto wparam = this->write_param.load(std::memory_order_acquire);
	auto rparam = this->read_param.load(std::memory_order_acquire);

	auto wpos = wparam.pos & this->_capacity;  //相当于求余
	auto wptr = this->_ptr + wpos;  //写指针起始位置。

	if(size == 0 || wparam.len != 0)
	{
		return std::span<char>(wptr, 0);
	}

	auto writableBytes = this->_capacity + rparam.pos - wparam.pos;  //剩余可写入的空间
	auto tailBytes = this->_capacity - wpos;  //写指针到尾部的空间
	if(tailBytes > writableBytes) //说明没有回绕
	{
		wparam.len = writableBytes > size ? size : writableBytes;
	}else
	{
		wparam.len = tailBytes > size ? size : tailBytes;
	}

	this->write_param.store(wparam, std::memory_order_release);
    return std::span<char>(wptr, wparam.len);
}

int RingBuffer::releaseWriteBuffer(unsigned size)
{
	if(size == 0)
	{
		return 0;
	}
	//这里必定不会产生回绕
	auto wparam = this->write_param.load(std::memory_order_acquire);
	if(wparam.len == 0)
	{
		return 0;
	}

	auto rparam = this->read_param.load(std::memory_order_acquire);

	auto wpos = wparam.pos & this->_capacity;  //相当于求余
	auto wptr = this->_ptr + wpos;  //写指针起始位置。

	auto releaseSize = wparam.len > size ? size : wparam.len;
	wparam.pos += releaseSize;
	wparam.len -= releaseSize;

	this->write_param.store(wparam, std::memory_order_release);
    return releaseSize;
}

int RingBuffer::releaseWriteBuffer()
{
    //这里必定不会产生回绕
	auto wparam = this->write_param.load(std::memory_order_acquire);
	if(wparam.len == 0)
	{
		return 0;
	}
	auto rparam = this->read_param.load(std::memory_order_acquire);

	auto wpos = wparam.pos & this->_capacity;  //相当于求余
	auto wptr = this->_ptr + wpos;  //写指针起始位置。
	auto releaseSize = wparam.len;
	wparam.pos += releaseSize;
	wparam.len -= releaseSize;

	this->write_param.store(wparam, std::memory_order_release);
    return releaseSize;
}

std::span<char> RingBuffer::getReadBuffer(unsigned size)
{
    auto wparam = this->write_param.load(std::memory_order_acquire);
	auto rparam = this->read_param.load(std::memory_order_acquire);

	auto rpos = wparam.pos & this->_capacity;  //相当于求余
	auto rptr = this->_ptr + rpos;  //写指针起始位置。

	if(size == 0 || rparam.len != 0)
	{
		return std::span<char>(rptr, 0);
	}

	auto redableBytes = wparam.pos - rparam.pos;  //剩余可写入的空间
	auto tailBytes = this->_capacity - rpos;  //写指针到尾部的空间
	if(tailBytes > redableBytes) //说明没有回绕
	{
		rparam.len = redableBytes > size ? size : redableBytes;
	}else
	{
		rparam.len = tailBytes > size ? size : tailBytes;
	}

	this->write_param.store(wparam, std::memory_order_release);
    return std::span<char>(rptr, rparam.len);
}

int RingBuffer::releaseReadBuffer(unsigned size)
{
    if(size == 0)
	{
		return 0;
	}
	//这里必定不会产生回绕
	auto wparam = this->write_param.load(std::memory_order_acquire);
	auto rparam = this->read_param.load(std::memory_order_acquire);
	if(rparam.len == 0)
	{
		return 0;
	}

	

	auto rpos = rparam.pos & this->_capacity;  //相当于求余
	auto rptr = this->_ptr + rpos;  //写指针起始位置。

	auto releaseSize = rparam.len > size ? size : rparam.len;
	rparam.pos += releaseSize;
	rparam.len -= releaseSize;

	this->read_param.store(rparam, std::memory_order_release);
    return releaseSize;
}

int RingBuffer::releaseReadBuffer()
{
    //这里必定不会产生回绕
	auto wparam = this->write_param.load(std::memory_order_acquire);
	auto rparam = this->read_param.load(std::memory_order_acquire);
	if(rparam.len == 0)
	{
		return 0;
	}
	

	auto rpos =rparam.pos & this->_capacity;  //相当于求余
	auto wptr = this->_ptr + rpos;  //写指针起始位置。
	auto releaseSize = rparam.len;
	rparam.pos += releaseSize;
	rparam.len -= releaseSize;

	this->read_param.store(rparam, std::memory_order_release);
    return releaseSize;
}
