#pragma once
#include<string>
#include<array>
#include<cstdint>
#include<cstring>
#include<stdexcept>

class BitConverter
{
public:


	static std::vector<int> toVec(unsigned value)
	{
		std::vector<int> vec;
		int num = 0;
		while (value > 0)
		{
			num++;
			if ((value & 0x1) == 0x1)
			{
				vec.push_back(num);
			}
			value >>= 1;
		}
		return vec;
	}
	//其他类型转字节数组
	static std::vector<int> getBitIndex(unsigned value)
	{
		std::vector<int> result;
		unsigned idx = 0;
		while (value) {
			if (value & 1u) result.push_back(idx);
			value >>= 1;
			++idx;
		}
		return result;
	}

	static unsigned setBitIndex(const std::vector<int>& inxs) {
		unsigned value = 0;
		for (unsigned i : inxs) 
			value |= (1u << i);
		return value;
	}


	//字节数组转其他类型(异常模式: 越界/空指针抛出 std::runtime_error)

	static double ToDouble(const char* value, int valueLen, int startIndex)
	{
		return Converter<double>(value, valueLen, startIndex);
	}

	static float ToSingle(const char* value, int valueLen, int startIndex)
	{
		return Converter<float>(value, valueLen, startIndex);
	}

	static std::int16_t ToInt16(const char* value, int valueLen, int startIndex)
	{
		return Converter<std::int16_t>(value, valueLen, startIndex);
	}

	static std::int32_t ToInt32(const char* value, int valueLen, int startIndex)
	{
		return Converter<std::int32_t>(value, valueLen, startIndex);
	}

	static std::int64_t ToInt64(const char* value, int valueLen, int startIndex)
	{
		return Converter<std::int64_t>(value, valueLen, startIndex);
	}

	static std::uint16_t ToUInt16(const char* value, int valueLen, int startIndex)
	{
		return Converter<std::uint16_t>(value, valueLen, startIndex);
	}

	static std::uint32_t ToUInt32(const char* value, int valueLen, int startIndex)
	{
		return Converter<std::uint32_t>(value, valueLen, startIndex);
	}

	static std::uint64_t ToUInt64(const char* value, int valueLen, int startIndex)
	{
		return Converter<std::uint64_t>(value, valueLen, startIndex);
	}

public:
	//返回
	template<typename T>
	static T Converter(const char* buffer, int bufferLen, int startIndex)
	{
		if (buffer == nullptr)
		{
			throw std::runtime_error("buffer is nullptr");
		}

		if (startIndex >= bufferLen)
		{
			throw std::runtime_error("startIndex must less than bufferLen");
		}
		int size = sizeof(T);
		if (startIndex > bufferLen - size)
		{
			throw std::runtime_error("do not have enough bytes");
		}

		T value;
		std::memcpy(&value, buffer + startIndex, sizeof(T));
		return value;
	}

	template<typename T>
	static T Converter(const char* buffer)
	{
		if (buffer == nullptr)
		{
			throw std::runtime_error("buffer is nullptr");
		}
		auto bufferlen = strlen(buffer);
		if (bufferlen < sizeof(T))
		{
			throw std::runtime_error("字符串长度不匹配");
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
