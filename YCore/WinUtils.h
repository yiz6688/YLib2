#pragma once
#include<Windows.h>
#include<format>
#include<string>
#include"Encoding.h"



class WinUtils
{
public:
	static std::string getError(const std::string& func)
	{
		auto code = GetLastError();
		return std::format("{} fail，code：{}", func, code);
	}


	static std::string getModuleDirectory()
	{
		wchar_t path[MAX_PATH];
		HMODULE hModule = NULL;
		auto ret = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCWSTR)(&getModuleDirectory), &hModule);
		//println("{}", ret);

		if (ret == FALSE)
		{
			return "";
		}

		memset(path, 0, sizeof(path));

		auto size = GetModuleFileNameW(hModule, path, MAX_PATH);

		auto value = Encoding::UTF16ToUTF8(std::wstring_view(path, size));
		return value;

	}

	static std::string getExeDirectory()
	{
		wchar_t path[MAX_PATH];
		// 传入 NULL 表示获取当前进程主模块的路径
		DWORD size = GetModuleFileNameW(NULL, path, MAX_PATH);

		auto value = Encoding::UTF16ToUTF8(std::wstring_view(path, size));
		return value;
	}

	


};

