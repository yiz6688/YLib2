#pragma once
#include<array>
#include<string>
#include<string_view>
#include<vector>


class Utils
{

public:

	static std::array<char,2> toHex(char ch);
	static char fromHex(std::array<char, 2> hex);


	static std::string toHex(const std::vector<char>& data);
	static std::string toHex(const std::string& data);
	static std::string toHex(std::string_view data);
	//将字符串转换为hex，
	static std::string toHex(const char* data, int size, std::string_view delimiter);


	static std::vector<char> fromHex(const std::vector<char>& hexData);
	static std::vector<char> fromHex(const std::string& hexStr);
	static std::vector<char> fromHex(std::string_view hexStr);
	static std::vector<char> fromHex(const char* hexStr, int size);

	static std::vector<int> getBitPos(unsigned val);
	
	static unsigned parseBitPos(std::vector<int> vec);

};