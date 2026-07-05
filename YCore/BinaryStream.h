#pragma once
#include"Stream.h"
#include<array>
#include<cctype>
#include<expected>
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

	std::expected<std::int16_t, std::string> readInt16()
	{
		return this->read<std::int16_t>();
	}

	std::expected<std::int32_t, std::string> readInt32()
	{
		return this->read<std::int32_t>();
	}

	std::expected<std::uint16_t, std::string> readUInt16()
	{
		return this->read<std::uint16_t>();
	}
	
	std::expected<std::uint32_t, std::string> readUInt32()
	{
		return this->read<std::uint32_t>();
	}

	std::expected<float, std::string> readSingle()
	{
		return this->read<float>();
	}

	std::expected<double, std::string> readDouble()
	{
		return this->read<double>();
	}

	std::expected<long, std::string> write(const char* data, int size)
	{
		return this->_stream->write(data, size);
	}

	std::expected<long, std::string> write(const std::string& data)
	{
		return this->_stream->write(data.c_str(), static_cast<int>(data.size()));
	}

	std::expected<void, std::string> flush()
	{
		return this->_stream->flush();
	}

	auto seek(long offset, SeekOrigin origin)
	{
		return this->_stream->seek(offset, origin);
	}




public:
	template<typename T, size_t N = sizeof(T)>
	std::expected<long, std::string> write(T val)
	{
		std::array<char, N> buffer;
		memcpy(buffer.data(), &val, sizeof(T));
		auto result = this->_stream->write(buffer.data(), N);
		if (!result) 
		{
			return std::unexpected(result.error());
		}
		if (result.value() != N) 
		{
			return std::unexpected("写入长度不足");
		}
		return result;
	}


	template<typename T, size_t N = sizeof(T)>
	std::expected<T, std::string> read()
	{
		std::array<char, N> buffer;
		auto result = this->_stream->read(buffer.data(), N);
		if (!result) 
		{
			return std::unexpected(result.error());
		}

		if (result.value() != N)
		{
			return std::unexpected("读取长度不足");
		}

		T val{};
		memcpy(&val, buffer.data(), sizeof(T));
		return val;
	}

	//异常版本，失败抛出异常
	template<typename T, size_t N = sizeof(T)>
	void tryWrite(T val)
	{
		std::array<char, N> buffer;
		memcpy(buffer.data(), &val, sizeof(T));
		auto result = this->_stream->write(buffer.data(), N);
		if (!result)
		{
			throw std::runtime_error(result.error());
		}
		if (result.value() != N)
		{
			throw std::runtime_error("写入长度不足");
		}
		return;
	}


	template<typename T, size_t N = sizeof(T)>
	T tryRead()
	{
		std::array<char, N> buffer;
		auto result = this->_stream->read(buffer.data(), N);
		if (!result)
		{
			throw std::runtime_error(result.error());
		}

		if (result.value() != N)
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