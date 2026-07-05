#include"RingBuffer.h"
#include<algorithm>


RingBuffer::RingBuffer(int bufferSize)
	:_capacity{ bufferSize }, _buffer{ new char[bufferSize] }, _ptr{ _buffer.get() }
{
	
}

RingBuffer::RingBuffer(char* buffer, int size)
	:_capacity{ size }, _buffer{ nullptr }, _ptr{ buffer }
{

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
	this->write_pos.store(value.write_pos.load());
	this->used_count.store(value.used_count.load());
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
	this->write_pos.store(value.write_pos.load());
	this->used_count.store(value.used_count.load());
	return *this;
}

int RingBuffer::read(char* buffer, int size)
{
	return this->read(buffer, 0, size);
}

int RingBuffer::read(char* buffer, int offset, int size)
{
	auto writePos = this->write_pos.load(std::memory_order_acquire);
	auto usedCount = this->used_count.load(std::memory_order_acquire);

	int readPos = writePos - usedCount;
	if (readPos < 0)
	{
		readPos += this->_capacity;
	}

	auto readBytes = usedCount;
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

	usedCount -= readBytes;
	this->used_count.store(usedCount, std::memory_order_release);//或者更新已用空间容量。


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
	auto writePos = this->write_pos.load(std::memory_order_acquire);
	auto usedCount = this->used_count.load(std::memory_order_acquire);

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

	usedCount += writeBytes;
	writePos += writeBytes;
	if (writePos > this->_capacity)
	{
		writePos -= this->_capacity;
	}
	if (usedCount > this->_capacity)
	{
		usedCount = this->_capacity;
	}

	this->used_count.store(usedCount, std::memory_order_release);//更新已用空间容量。
	this->write_pos.store(writePos, std::memory_order::release);

	return writeBytes;
}

int RingBuffer::readFrom(RingBuffer& ring, int size)
{
	int nCapacity = ring.getCapacity();
	int nReadPos = ring.getReadPos();
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

