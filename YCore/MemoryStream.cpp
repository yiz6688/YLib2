#include "MemoryStream.h"
//#include"../utils/numUtils.h"
#include <stdexcept>
#include <algorithm>

MemoryStream::MemoryStream()
	:MemoryStream(1024)
{}

MemoryStream::MemoryStream(int size)
	:_capacity{ size }, _length{ 0 }, _origin{ 0 }, _expandable{ true }
{
	this->_writeable = true;
	this->_readable = true;
	//size = nextpow2(size);
	this->_buffer = std::make_unique<char[]>(size);
	this->_ptr = this->_buffer.get();
}

MemoryStream::MemoryStream(char* data, int dataLen, int offset, int count, bool visiable = false)
	:_buffer{nullptr}, _ptr{data},_capacity{ dataLen }, _length{ dataLen }, _origin{ offset }, _expandable{ visiable }
{
	this->_writeable = true;
	this->_readable = true;
}

MemoryStream::~MemoryStream()
{
	this->_ptr = nullptr;
}

std::expected<long, std::string> MemoryStream::getLength()
{
	return this->_length - this->_origin;
}

std::expected<void, std::string> MemoryStream::setLength(long value)
{
	return std::unexpected("不允许设置长度");
}

std::expected<long, std::string> MemoryStream::getPosition()
{
	return this->_position - this->_origin;
}

std::expected<void, std::string> MemoryStream::setPosition(long value)
{
	if (value < 0)
	{
		return std::unexpected("设置值必须是正数");
	}

	this->_position += value;
	return {};
}

long MemoryStream::getCapacity()
{
	return this->_capacity - this->_origin;
}

std::expected<long, std::string> MemoryStream::setCapacity(long value)
{
	if (value <= this->_length)
	{
		return std::unexpected("新容量不能小于当前长度");
	}
	if (!this->_expandable && value != this->_capacity)
	{
		return std::unexpected("流不支持扩容");
	}
	if (!this->_expandable || value == _capacity)
	{
		return this->_capacity;
	}

	if (value > 0)
	{
		std::unique_ptr<char[]> array = std::make_unique<char[]>(value);
		if (this->_length > 0)
		{
			std::copy_n(this->_ptr, this->_length, array.get());
		}
		this->_buffer = std::move(array);
		this->_ptr = this->_buffer.get();
	}
	else
	{
		this->_buffer.release();
		this->_ptr = nullptr;
	}
	this->_capacity = value;
	return value;
}


std::expected<void, std::string> MemoryStream::flush()
{
	return {};
}

std::expected<long, std::string> MemoryStream::seek(long offset, SeekOrigin origin)
{
	long position = this->_position;
	if (origin == SeekOrigin::Begin)
	{
		int num3 = this->_origin + offset;
		if (offset < 0 || num3 < this->_origin)
		{
			return std::unexpected("seek超过了起始位置");  //发生了溢出
		}
		this->_position = num3;
	}else if (origin == SeekOrigin::Current)
	{
		int num2 = this->_position + offset;
		if (num2 < this->_origin)
		{
			return std::unexpected("seek超过了起始位置");  //发生了溢出
		}
		this->_position = num2;
	}
	else if (origin == SeekOrigin::End)
	{
		int num = this->_length + offset;
		if (num < this->_origin)
		{
			return std::unexpected("seek超过了起始位置");  //发生了溢出
		}
		this->_position = num;
	}
	else
	{
		return std::unexpected("origin参数不合法");
	}
	return this->_position;
}

std::expected<void, std::string> MemoryStream::close()
{
	return{};
}

std::expected<long, std::string> MemoryStream::basic_read(char* buffer, int size, int offset, int count)
{
	if (this->canRead() == false)
	{
		return std::unexpected("不支持读取");
	}

	if (offset + count > size)
	{
		throw std::invalid_argument("传递参数不合法");
	}
	//剩余空间
	int num = this->_length - this->_position;
	if (num > count)
	{
		num = count;
	}
	if(num <= 0)
	{
		return 0;
	}
	if (num <= 8)
	{
		int num2 = num;
		while (--num2 >= 0)
		{
			buffer[offset + num2] = this->_ptr[this->_position + num2];
		}
	}
	else
	{
		std::copy_n(this->_ptr + this->_position, num, buffer + offset);
	}
	this->_position += num;
	return num;
}


std::expected<long, std::string> MemoryStream::basic_write(const char* buffer, int size, int offset, int count)
{

	if (this->canWrite() == false)
	{
		return std::unexpected("不支持写入");
	}

	if (offset + count > size)
	{
		return std::unexpected("传递参数不合法");
	}

	int num = this->_position + count;
	if (num < 0)
	{
		return std::unexpected("position overflow");  //读取越界了，太大了,很大的正数就是负数
	}

	if (num > this->_length)
	{
		//bool flag = this->_position > this->_length;
		if (num > this->_capacity)
		{
			if (this->ensureCapacity(num))
			{
				//flag = false;
			}
			else
			{
				return std::unexpected("扩容失败！");
			}
		}

		//if (flag)
		//{
			//中间空出来的部分按0处理
			std::fill_n(this->_ptr + this->_length, num - this->_length, 0);
			this->_length = num;
			//return 0;
		//}
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
		std::copy_n(buffer + offset, count, this->_ptr + this->_position);
	}

	this->_position = num;

	return count;
}

bool MemoryStream::ensureCapacity(long value)
{
	if (value <= 0)
	{
		return false;
	}
	if (value > this->_capacity)
	{
		int num = value;
		if (num < 256)
		{
			num = 256;
		}
		if (num <  this->_capacity * 2)
		{
			num = _capacity * 2;
		}

		if ((int)(this->_capacity * 2) > 2147483591u)
		{
			num = ((value > 2147483591) ? value : 2147483591);
		}
		
		auto result = this->setCapacity(num);
		return result.operator bool();
	}

	return true;
}
