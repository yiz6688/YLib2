#include "Utils.h"

std::array<char, 2> Utils::toHex(char ch)
{
    std::array<char, 2> hex{'0', '0'};
    int high = (ch >> 4) & 0x0F;
    int low = ch & 0x0F;

    hex[0] = high < 10 ? '0' + high : 'A' + (high - 10);
    hex[1] = low < 10 ? '0' + low : 'A' + (low - 10);
    return hex;
}

char Utils::fromHex(std::array<char, 2> hex)
{
    int high = hex[0] >= 'A' ? hex[0] - 'A' + 10 : hex[0] - '0';
    int low = hex[1] >= 'A' ? hex[1] - 'A' + 10 : hex[1] - '0';
    return (high << 4) | low;
}




std::string Utils::toHex(const std::vector<char>& data)
{
	return toHex(data.data(), static_cast<int>(data.size()), "");
}

std::string Utils::toHex(const std::string& data)
{
	return toHex(data.data(), static_cast<int>(data.size()), "");
}

std::string Utils::toHex(std::string_view data)
{
	return toHex(data.data(), static_cast<int>(data.size()), "");
}

std::string Utils::toHex(const char* data, int size, std::string_view delimiter)
{
    std::string hex;
    for (int i = 0; i < size; i++)
    {
        auto hexPair = toHex(data[i]);
        hex.append(hexPair.data(), 2);
        if (i < size - 1)
        {
            hex.append(delimiter);
        }
    }
    return hex;
}

std::vector<char> Utils::fromHex(const std::vector<char>& hexData)
{
	return fromHex(hexData.data(), hexData.size());
}

std::vector<char> Utils::fromHex(const std::string& hexStr)
{
	return fromHex(hexStr.data(), hexStr.size());
}

std::vector<char> Utils::fromHex(std::string_view hexStr)
{
	return fromHex(hexStr.data(), hexStr.size());
}

std::vector<char> Utils::fromHex(const char* hexStr, int size)
{
    std::vector<char> data;
	for (int i = 0; i < size; i++)
    {
		auto ch = hexStr[i];
        if ( (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f') )
        {
            if (i % 2 == 1)
            {
				auto ptr = hexStr + i - 1;
                std::array<char, 2> arr{ ptr[0], ptr[1] };
				ch = fromHex(arr);
                data.push_back(ch);
            }
        }
        else
        {
            break;
        }
    }
    return data;
}

std::vector<int> Utils::getBitPos(unsigned value)
{
    std::vector<int> vec;
    int num = 0;
    while (value > 0)
    {
        
        if ((value & 0x1) == 0x1)
        {
            vec.push_back(num);
        }
        value >>= 1;
        num++;
    }
    return vec;
}

unsigned Utils::parseBitPos(std::vector<int> vec)
{
    unsigned value = 0;
    for (int val : vec)
    {
        if (val > 32)
        {
            return value;
        }
        value |= (1 << val);
    }
    return value;
}
