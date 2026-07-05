#pragma once
#include<string>
#include<vector>
#include<expected>

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

	Stream& operator=(const Stream&) = default;

	Stream& operator=(Stream&& other) = default;

	virtual ~Stream() = default;

public:
	virtual std::expected<void, std::string> close() = 0;
	//获取流长度
	virtual std::expected<long, std::string> getLength() = 0;

	virtual std::expected<void, std::string> setLength(long length) = 0;

	//获取流当前的位置
	virtual  std::expected<long, std::string> getPosition() = 0;
	//设置流的位置
	virtual std::expected<void, std::string> setPosition(long position) = 0;

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
	virtual std::expected<void, std::string> flush() = 0;
	//设置流的位置，根据origin参数，偏移offset个字节
	virtual  std::expected<long, std::string> seek(long offset, SeekOrigin origin) = 0;


	//读取流内容到缓冲区，返回实际读取的字节数
	std::expected<long, std::string> read(char* buffer, int size, int offset, int count);
	//读取流内容到缓冲区，返回实际读取的字节数
	std::expected<long, std::string> read(char* buffer, int size);

	std::expected<long, std::string> read(std::vector<char>& vec);


	std::expected<long, std::string> write(const std::string& str);

	std::expected<long, std::string> write(const std::vector<char>& vec);
	//将缓冲区内容写入流，返回实际写入的字节数
	std::expected<long, std::string> write(const char* data, int size, int offset, int count);
	//将缓冲区内容写入流，返回实际写入的字节数
	std::expected<long, std::string> write(const char* data, int size);

protected:
	//c++重载后，同名函数就不显示了，设计两个基本的读写函数，其余的调用这两个函数。
	//基本写方法
	virtual  std::expected<long, std::string> basic_write(const char* data, int size, int offset, int count) = 0;
	//基本读方法
	virtual  std::expected<long, std::string> basic_read(char* buffer, int size, int offset, int count) = 0;

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