#include "MemoryStream.h"
#include <stdexcept>
#include <algorithm>
#include<cstring>

namespace
{
	//Array.MaxLength 的等价物：允许分配的最大字节数(比 INT_MAX 略小，预留对齐等开销)
	constexpr long MaxStreamLength = 2147483591L;
	constexpr const char* ClosedMsg = "流已关闭";
}

MemoryStream::MemoryStream()
	:MemoryStream(1024)
{}

MemoryStream::MemoryStream(int size)
	:_capacity{ size }, _length{ 0 }, _origin{ 0 }, _expandable{ true }, _isOpen{ true }
{
	if (size < 0)
	{
		throw std::invalid_argument("容量不能为负数");
	}
	this->_writeable = true;
	this->_readable = true;
	this->_seekable = true;
	this->_buffer = std::make_unique<char[]>(size);
	this->_ptr = this->_buffer.get();
}

MemoryStream::MemoryStream(char* data, int dataLen, int offset, int count, bool visiable)
	:_buffer{ nullptr }, _ptr{ data }, _capacity{ 0 }, _length{ 0 }, _origin{ 0 }, _expandable{ false }, _isOpen{ true }
{
	if (data == nullptr || dataLen < 0 || offset < 0 || count < 0
		|| offset > dataLen - count)
	{
		throw std::runtime_error("入参错误");
	}

	this->_origin = offset;
	this->_capacity = offset + count;   //与 .NET 一致：视图流的容量等于视图长度
	this->_length = offset + count;
	this->_expandable = visiable;       //保留原设计：该参数决定视图流是否可扩容
	this->_writeable = true;
	this->_readable = true;
	this->_seekable = true;
	this->_position = offset;
}

MemoryStream::~MemoryStream()
{
	this->_ptr = nullptr;
}

long MemoryStream::getLength()
{
	if (!this->_isOpen)
	{
		throw std::runtime_error(ClosedMsg);
	}
	return this->_length - this->_origin;
}

void MemoryStream::setLength(long value)
{
	if (!this->_isOpen)
	{
		throw std::runtime_error(ClosedMsg);
	}
	if (value < 0)
	{
		throw std::runtime_error("设置长度不允许小于0");
	}
	if (value > MaxStreamLength || value > MaxStreamLength - this->_origin)
	{
		throw std::runtime_error("长度超出最大限制");
	}

	long newLength = this->_origin + value;
	bool realloced = this->ensureCapacity(newLength);  //失败抛出异常
	//没有新分配数组时，扩展出的部分需要显式清零
	if (!realloced && newLength > this->_length)
	{
		std::memset(this->_ptr + this->_length, 0, newLength - this->_length);
	}
	this->_length = newLength;
	if (this->_position > this->_length)
	{
		this->_position = this->_length;
	}
}

long MemoryStream::getPosition()
{
	if (!this->_isOpen)
	{
		throw std::runtime_error(ClosedMsg);
	}
	return this->_position - this->_origin;
}

void MemoryStream::setPosition(long value)
{
	if (!this->_isOpen)
	{
		throw std::runtime_error(ClosedMsg);
	}
	if (value < 0)
	{
		throw std::runtime_error("设置值必须是正数");
	}
	if (value > MaxStreamLength - this->_origin)
	{
		throw std::runtime_error("位置超出最大限制");
	}

	this->_position = this->_origin + value;
}

long MemoryStream::getCapacity()
{
	if (!this->_isOpen)
	{
		return -1;
	}
	return this->_capacity - this->_origin;
}

long MemoryStream::setCapacity(long value)
{
	if (!this->_isOpen)
	{
		throw std::runtime_error(ClosedMsg);
	}
	if (value < 0)
	{
		throw std::runtime_error("容量不能为负数");
	}
	if (value < this->_length - this->_origin)
	{
		throw std::runtime_error("新容量不能小于当前长度");
	}
	if (value > MaxStreamLength - this->_origin)
	{
		throw std::runtime_error("容量超出最大限制");
	}
	if (!this->_expandable && value != this->_capacity - this->_origin)
	{
		throw std::runtime_error("流不支持扩容");
	}

	long newCapacity = this->_origin + value;
	if (newCapacity != this->_capacity)
	{
		std::unique_ptr<char[]> array = std::make_unique<char[]>(newCapacity);
		if (this->_length > 0)
		{
			std::copy_n(this->_ptr, this->_length, array.get());
		}
		this->_buffer = std::move(array);
		this->_ptr = this->_buffer.get();
		this->_capacity = newCapacity;
	}
	return this->_capacity - this->_origin;
}


void MemoryStream::flush()
{
	return;
}

long MemoryStream::seek(long offset, SeekOrigin origin)
{
	if (!this->_isOpen)
	{
		throw std::runtime_error(ClosedMsg);
	}

	long base;
	if (origin == SeekOrigin::Begin)
	{
		base = this->_origin;
	}
	else if (origin == SeekOrigin::Current)
	{
		base = this->_position;
	}
	else if (origin == SeekOrigin::End)
	{
		base = this->_length;
	}
	else
	{
		throw std::runtime_error("origin参数不合法");
	}

	long long sum = static_cast<long long>(base) + offset;
	if (sum < this->_origin)
	{
		throw std::runtime_error("seek超过了起始位置");
	}
	if (sum > MaxStreamLength)
	{
		throw std::runtime_error("seek超出最大位置");
	}
	this->_position = static_cast<long>(sum);
	return this->_position - this->_origin;
}

void MemoryStream::close()
{
	this->_isOpen = false;
	this->_readable = false;
	this->_writeable = false;
	this->_seekable = false;
	this->_expandable = false;
}

long MemoryStream::basic_read(char* buffer, int size, int offset, int count)
{
	if (!this->_isOpen)
	{
		throw std::runtime_error(ClosedMsg);
	}
	if (this->canRead() == false)
	{
		throw std::runtime_error("不支持读取");
	}
	if (offset < 0 || size < 0 || count < 0 || offset > size - count)
	{
		throw std::runtime_error("输入参数不合法");
	}
	//剩余空间
	auto num = this->_length - this->_position;
	if (num > count)
	{
		num = count;
	}
	if (num <= 0)
	{
		return 0;
	}
	if (num <= 8)
	{
		auto num2 = num;
		while (--num2 >= 0)
		{
			buffer[offset + num2] = this->_ptr[this->_position + num2];
		}
	}
	else
	{
		std::memmove(buffer + offset, this->_ptr + this->_position, num);
	}
	this->_position += num;
	return num;
}


long MemoryStream::basic_write(const char* buffer, int size, int offset, int count)
{
	if (!this->_isOpen)
	{
		throw std::runtime_error(ClosedMsg);
	}
	if (this->canWrite() == false)
	{
		throw std::runtime_error("不支持写入");
	}
	if (offset < 0 || size < 0 || count < 0 || offset > size - count)
	{
		throw std::runtime_error("输入参数不合法");
	}
	if (count == 0)
	{
		return 0;
	}

	long long i = static_cast<long long>(this->_position) + count;
	if (i > MaxStreamLength)
	{
		throw std::runtime_error("位置溢出");
	}

	if (i > this->_length)
	{
		bool mustZero = this->_position > this->_length;
		if (i > this->_capacity)
		{
			bool realloced = this->ensureCapacity(static_cast<long>(i));  //失败抛出异常
			if (realloced)
			{
				mustZero = false;  //新数组本身已被清零
			}
		}
		if (mustZero)
		{
			std::memset(this->_ptr + this->_length, 0, static_cast<long>(i) - this->_length);
		}
		this->_length = static_cast<long>(i);
	}

	if (count <= 8 && buffer != this->_ptr)
	{
		int num2 = count;
		while (--num2 >= 0)
		{
			this->_ptr[this->_position + num2] = buffer[offset + num2];
		}
	}
	else
	{
		std::memmove(this->_ptr + this->_position, buffer + offset, count);
	}

	this->_position = static_cast<long>(i);

	return count;
}

bool MemoryStream::ensureCapacity(long value)
{
	if (value < 0)
	{
		throw std::runtime_error("容量不能为负数");
	}
	if (value > MaxStreamLength)
	{
		throw std::runtime_error("容量超出最大限制");
	}
	if (value <= this->_capacity)
	{
		return false;
	}
	if (!this->_expandable)
	{
		throw std::runtime_error("流不支持扩容");
	}

	long newCapacity = (std::max)(value, 256L);
	if (this->_capacity <= MaxStreamLength / 2)
	{
		long doubled = this->_capacity * 2;
		if (newCapacity < doubled)
		{
			newCapacity = doubled;
		}
	}
	if (newCapacity > MaxStreamLength)
	{
		newCapacity = MaxStreamLength;
	}
	if (newCapacity < value)
	{
		newCapacity = value;
	}

	this->setCapacity(newCapacity - this->_origin);
	return true;
}
