#pragma once

#include<Windows.h>
#include<string>
#include<string_view>

class Encoding
{

public:




public:
	static std::wstring GBKToUTF16(std::string_view gbk);

	static std::string UTF16ToGBK(std::wstring_view utf16);

	static std::wstring UTF8ToUTF16(std::string_view utf8);

	static std::string UTF16ToUTF8(std::wstring_view utf16);

	static std::string GBKToUTF8(std::string_view gbk);

	static std::string UTF8ToGBK(std::string_view utf8);




};