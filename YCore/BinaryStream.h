#pragma once
#include"Stream.h"
#include<array>
#include<cstring>
#include<cstdint>
#include<stdexcept>

class BinaryStream
{

public:
	BinaryStream(Stream* stream)
		:_stream(stream)
	{

	}

public:

	std::int16_t readInt16()
	{
		return this->read<std::int16_t>();
	}

	std::int32_t readInt32()
	{
		return this->read<std::int32_t>();
	}

	std::uint16_t readUInt16()
	{
		return this->read<std::uint16_t>();
	}

	std::uint32_t readUInt32()
	{
		return this->read<std::uint32_t>();
	}

	float readFloat32()
	{
		return this->read<float>();
	}

	double readFloat64()
	{
		return this->read<double>();
	}

	long write(const char* data, int size)
	{
		return this->_stream->write(data, size);
	}

	long write(const std::string& data)
	{
		return this->_stream->write(data.c_str(), static_cast<int>(data.size()));
	}

	void flush()
	{
		this->_stream->flush();
	}

	long seek(long offset, SeekOrigin origin)
	{
		return this->_stream->seek(offset, origin);
	}


public:
	template<typename T, size_t N = sizeof(T)>
	long write(T val)
	{
		std::array<char, N> buffer;
		memcpy(buffer.data(), &val, sizeof(T));
		long n = this->_stream->write(buffer.data(), N);  //失败抛出异常
		if (n != N)
		{
			throw std::runtime_error("写入长度不足");
		}
		return n;
	}

	template<typename T, size_t N = sizeof(T)>
	T read()
	{
		std::array<char, N> buffer;
		long n = this->_stream->read(buffer.data(), N);  //失败抛出异常
		if (n != N)
		{
			throw std::runtime_error("读取长度不足");
		}
		T val{};
		memcpy(&val, buffer.data(), sizeof(T));
		return val;
	}

private:
	Stream* _stream;
};
