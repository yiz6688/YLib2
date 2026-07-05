#pragma once

#ifndef DLLTEST_EXPORTS
    #define DLLTEST_API __declspec(dllimport)
#else
    #define DLLTEST_API __declspec(dllexport)
#endif

extern "C"
{

    DLLTEST_API void* openFile(const char* filepath, int mode, int access);
    DLLTEST_API void closeFile(void* handle);
    DLLTEST_API long readFile(void* handle, char* buffer, int size);
    DLLTEST_API long writeFile(void* handle, const char* buffer, int size);
    DLLTEST_API long seekFile(void* handle, long offset, int origin);
    DLLTEST_API void flushFile(void* handle);   
    DLLTEST_API long getFilePosition(void* handle);

}