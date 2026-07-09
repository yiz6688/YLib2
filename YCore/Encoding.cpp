#include "Encoding.h"
#include<string>
#include<stdexcept>
#include<vector>


using namespace std;



std::wstring Encoding::GBKToUTF16(string_view gbk)
{

	//多字节转成utf16
	auto len = MultiByteToWideChar(CP_ACP, 0, gbk.data(), -1, NULL, 0);
	if (len <= 0)
	{
		throw runtime_error("转换失败");
	}
	std::wstring utf16;
	utf16.resize(len - 1);

	len = MultiByteToWideChar(CP_ACP, 0, gbk.data(), -1, utf16.data(), len - 1);
	return utf16;
}

std::string Encoding::UTF16ToGBK(wstring_view utf16)
{

	auto len = WideCharToMultiByte(CP_ACP, 0, utf16.data(), -1, NULL, 0, NULL, NULL);

	std::string gbk;
	gbk.resize(len - 1);
	//原始字符串使用-1 自动计算长度
	len = WideCharToMultiByte(CP_ACP, 0, utf16.data(), -1, gbk.data(), len - 1, NULL, NULL);

	return gbk;
}


std::wstring Encoding::UTF8ToUTF16(string_view utf8)
{
	//多字节转成utf16
	auto len = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), -1, NULL, 0);
	if (len <= 0)
	{
		throw runtime_error("转换失败");
	}
	std::wstring utf16;
	utf16.resize(len - 1);

	len = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), -1, utf16.data(), len - 1);
	return utf16;
}

std::string Encoding::UTF16ToUTF8(wstring_view utf16)
{
	auto len = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), -1, NULL, 0, NULL, NULL);

	std::string utf8;
	utf8.resize(len - 1);
	//原始字符串使用-1 自动计算长度
	len = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), -1, utf8.data(), len - 1, NULL, NULL);

	return utf8;
}

std::string Encoding::GBKToUTF8(std::string_view gbk)
{
	auto utf16 = Encoding::GBKToUTF16(gbk);

	return Encoding::UTF16ToUTF8(utf16.data());
}

std::string Encoding::UTF8ToGBK(std::string_view utf8)
{
    auto utf16 = Encoding::UTF8ToUTF16(utf8);

	return Encoding::UTF16ToGBK(utf16.data());
}
