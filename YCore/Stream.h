#pragma once
#include"base_config.hpp"
#include"TResult.h"
#include<string>
#include<vector>
#include<stdexcept>

enum class SeekOrigin
{
	Begin = 0,   //FILE_BEGIN    0   文件的开头或零点 指定了这个值，偏移会解释称无符号数

	Current,   //FILE_CURRENT    1

	End        //FILE_END    2
};

class Stream
{

public:
	Stream();

	Stream(const Stream&) = delete;

	Stream(Stream&& other) noexcept;

	Stream& operator=(const Stream&) = delete;

	Stream& operator=(Stream&& other) = default;

	virtual ~Stream() = default;

public:
	//正常关闭: 失败抛出 std::runtime_error(内部转调 inner_close)
	virtual void close();

	//获取流长度
	virtual long getLength() = 0;

	virtual void setLength(long length) = 0;

	//获取流当前的位置
	virtual long getPosition() = 0;
	//设置流的位置
	virtual void setPosition(long position) = 0;

	virtual bool canRead()
	{
		return this->_readable;
	}

	virtual bool canWrite()
	{
		return this->_writeable;
	}

	virtual bool canSeek()
	{
		return this->_seekable;
	}
	//将缓冲区内容写入基础设备
	virtual void flush() = 0;
	//设置流的位置，根据origin参数，偏移offset个字节
	virtual long seek(long offset, SeekOrigin origin) = 0;


	//读取流内容到缓冲区，返回实际读取的字节数
	long read(char* buffer, int size, int offset, int count);
	//读取流内容到缓冲区，返回实际读取的字节数
	long read(char* buffer, int size);

	long read(std::vector<char>& vec);


	long write(const std::string& str);

	long write(const std::vector<char>& vec);
	//将缓冲区内容写入流，返回实际写入的字节数
	long write(const char* data, int size, int offset, int count);
	//将缓冲区内容写入流，返回实际写入的字节数
	long write(const char* data, int size);

protected:
	//底层关闭实现: 返回 expected, 供 close()(抛异常) 与 析构/移动赋值(静默) 复用
	virtual TResult<void> inner_close() = 0;

	//c++重载后，同名函数就不显示了，设计两个基本的读写函数，其余的调用这两个函数。
	//基本写方法
	virtual long basic_write(const char* data, int size, int offset, int count) = 0;
	//基本读方法
	virtual long basic_read(char* buffer, int size, int offset, int count) = 0;

	void copyTo(Stream& stream);

private:
	void InternalCopyTo(Stream& stream, int bufferSize);

protected:
	//当前指针所在位置
	long _position = 0;
	//可读标志
	bool _readable;
	//可写标志
	bool _writeable;
	//可移动标志
	bool _seekable;
};
