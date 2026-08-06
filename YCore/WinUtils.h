#pragma once
#include<Windows.h>
#include<format>
#include<string>
#include<string_view>
#include"Encoding.h"
#include<print>
#include"TResult.h"


class WinUtils
{
public:
	static std::string getError(const std::string& func)
	{
		auto code = GetLastError();
		return std::format("{} fail,code:{}", func, code);
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

	
	static TResult<void> makeDirs(std::wstring_view wpath)
	{

		auto size = GetFullPathNameW(wpath.data(), 0, nullptr, nullptr);
		if(size == 0)
		{
			auto error = getError("GetFullPathNameW");
			return std::unexpected(error);
		}
		std::wstring fullPath(size, L'\0');
		size = GetFullPathNameW(wpath.data(), size, fullPath.data(), nullptr);
		if(size == 0)
		{
			auto error = getError("GetFullPathNameW");
			return std::unexpected(error);
		}

		
		std::wstring path;
		auto ret = FALSE;
		auto pos = fullPath.find_first_of(L"\\/");
		if(pos == std::wstring_view::npos)
		{
			return std::unexpected("Invalid path");
		}else if(pos !=0)
		{
			if(fullPath[pos -1] == L':')  //驱动器目录
			{
				pos++;
			}
		}

		while(true)
		{
			pos = fullPath.find_first_of(L"\\/", pos);
			if (pos == std::wstring_view::npos)
			{
				break;
			}
			path = fullPath.substr(0, pos);
			ret = CreateDirectoryW(path.data(), NULL);
			if(ret == FALSE)
			{
				int code = GetLastError();
				if(code != ERROR_ALREADY_EXISTS)
				{
					std::string error = std::format("CreateDirectoryW fail,code:{}", code);
					return std::unexpected(error);
				}
			}
			pos++;
		}
		return {};
	}

};

