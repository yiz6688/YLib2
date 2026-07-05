#define DLLTEST_EXPORTS
#include "DllTest.h"

DLLTEST_API void *openFile(const char *filepath, int mode, int access)
{
    return nullptr;
}

DLLTEST_API void closeFile(void *handle)
{
    return;
}

DLLTEST_API long readFile(void *handle, char *buffer, int size)
{
    return 0;
}

DLLTEST_API long writeFile(void *handle, const char *buffer, int size)
{
    return 0;
}

DLLTEST_API long seekFile(void *handle, long offset, int origin)
{
    return 0;
}

DLLTEST_API void flushFile(void *handle)
{
    return ;
}

DLLTEST_API long getFilePosition(void *handle)
{
    return 0;
}
