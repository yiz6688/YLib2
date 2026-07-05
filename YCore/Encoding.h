#pragma once

#include<Windows.h>
#include<string>
#include<string_view>

class Encoding
{

public:




public:
	static std::wstring GBK2UTF16(std::string_view gbk);

	static std::wstring UTF82UTF16(std::string_view utf8);

	static std::string UTF162GBK(std::wstring_view utf16);

	static std::string UTF162UTF8(std::wstring_view utf16);

};