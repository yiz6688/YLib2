#pragma once
#include<expected>
#include<string>
#include<array>
#include<cstdint>
#include<cstring>

class BitConverter
{
public:

	//其他类型转字节数组

	//字节数组转其他类型

	static std::expected<double, std::string> ToDouble(const char* value, int valueLen, int startIndex)
	{
		return Converter<double>(value, valueLen, startIndex);
	}

	static std::expected<float, std::string> ToSingle(const char* value,int valueLen, int startIndex)
	{
		return Converter<float>(value, valueLen, startIndex);
	}

	static std::expected<std::int16_t, std::string> ToInt16(const char* value, int valueLen, int startIndex)
	{
		return Converter<std::int16_t>(value, valueLen, startIndex);
	}

	static std::expected<std::int32_t, std::string> ToInt32(const char* value, int valueLen, int startIndex)
	{
		return Converter<std::int32_t>(value, valueLen, startIndex);
	}

	static std::expected<std::int64_t, std::string> ToInt64(const char* value, int valueLen, int startIndex)
	{
		return Converter<std::int64_t>(value, valueLen, startIndex);
	}

	static std::expected<std::uint16_t, std::string> ToUInt16(const char* value, int valueLen, int startIndex)
	{
		return Converter<std::uint16_t>(value, valueLen, startIndex);
	}

	static std::expected<std::uint32_t, std::string> ToUInt32(const char* value, int valueLen, int startIndex)
	{
		return Converter<std::uint32_t>(value, valueLen, startIndex);
	}

	static std::expected<std::uint64_t, std::string> ToUInt64(const char* value, int valueLen, int startIndex)
	{
		return Converter<std::uint64_t>(value, valueLen, startIndex);
	}

public:
	//返回
	template<typename T>
	static  std::expected<T, std::string> Converter(const char* buffer, int bufferLen, int startIndex)
	{
		if (buffer == nullptr)
		{
			return std::unexpected("buffer is nullptr");
		}

		if (startIndex >= bufferLen)
		{
			return std::unexpected("startIndex must less than bufferLen");
		}
		int size = sizeof(T);
		if (startIndex > bufferLen - size)
		{
			return std::unexpected("do not have enough bytes ");
		}

		T value;
		std::memcpy(&value, buffer + startIndex, sizeof(T));
		return value;
	}

	template<typename T>
	static  std::expected<T, std::string> Converter(const char* buffer)
	{
		if (buffer == nullptr)
		{
			return std::unexpected("buffer is nullptr");
		}
		auto bufferlen = strlen(buffer);
		if (bufferlen < sizeof(T))
		{
			return std::unexpected("字符串长度不匹配");
		}

		T value;
		std::memcpy(&value, buffer, sizeof(T));
		return value;
	}

	template<typename T, int N = sizeof(T)>
	static std::array<char, N> GetBytes(T value)
	{
		std::array<char, N> arr;
		char* ptr = reinterpret_cast<char*>(&value);
		std::copy(ptr, ptr + N, arr.begin());
		return arr;
	}

};
