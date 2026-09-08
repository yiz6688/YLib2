#pragma once
#include"Stream.h"
#include<string_view>
#include<string>
#include<memory>
#include"TResult.h"

//#define CREATE_NEW          1
//#define CREATE_ALWAYS       2
//#define OPEN_EXISTING       3
//#define OPEN_ALWAYS         4
//#define TRUNCATE_EXISTING   5

enum class FileMode
{
	//创建新文件,如果文件已存在需要抛出异常
	CreateNew = 0x1,      //CREATE_NEW          1
	//创建一个新文件，如果文件已存在，就覆写它，要求有写入权限
	Create,				//CREATE_ALWAYS       2
	//打开一个现有的文件，如果文件不存在需要抛出异常
	Open,					//OPEN_EXISTING       3
	//如果文件存在则打开文件，如果文件不存在则创建文件
	OpenOrCreate,			//OPEN_ALWAYS         4
	//打开一个现有的文件，并将其大小截断为零字节,文件的其他信息存在
	//不能从中读取，待验证。。
	Truncate,				//TRUNCATE_EXISTING   5
	//如果文件存在，打开文件并将光标移到文件末尾，否则创建新文件
	//Append只能与Write一起使用
	Append

};


enum class FileAccess
{
	//读文件权限
	Read = 0x1,
	//写文件权限
	Write = 0x2,
	//读写文件权限
	ReadWrite = 0x3,
	//不访问文件只获取信息
	None = 0x0
};

enum class FileShare
{
	//允许其他进程打开文件进行读取
	Read = 1,
	//允许其他进程打开文件进行写入
	Write,
	//允许其他进程打开文件进行读取和写入
	ReadWrite,
	//不允许其他进程打开文件
	None = 0x0
};


class FileStream final: public Stream
{
private:

	FileStream(const std::string_view filepath, FileMode fileMode, FileAccess fileAccess, FileShare fileShare, int bufferSize);

	FileStream(const std::string_view filepath, FileMode fileMode, FileAccess fileAccess);

	FileStream(const std::string_view filepath, FileMode fileMode);

public:
	FileStream(FileStream&& other) noexcept;

	FileStream& operator=(FileStream&& other) noexcept;

	~FileStream();

	//异常模式: close() 失败抛出 std::runtime_error; 析构/移动赋值静默(内部 inner_close)
	void setLength(long value) override;

	long getLength() override;

	long getPosition() override;

	void setPosition(long value) override;

	void flush() override;

	void flush(bool flushToDisk);

	long seek(long offset, SeekOrigin origin) override;

protected:
	//底层关闭实现(尽力 flush + 关句柄), 返回 expected; close() 抛异常, 析构/移动赋值静默
	std::expected<void, std::string> inner_close() override;

	long basic_read(char* data, int size, int offset, int count) override;

	long basic_write(const  char* data, int size, int offset, int count) override;




private:
	void flushRead();

	long flushWrite();

	long writeCore(const char* data, int size, int offset, int count);

	long readCore(char* data, int size, int offset, int count);

	long seekCore(long offset, SeekOrigin origin);

public:
	void init();

public:
	static TPtr<FileStream> create(std::string_view filepath, FileMode fileMode, 
		FileAccess fileAccess, FileShare fileShare, int bufferSize);

	static TPtr<FileStream> create(std::string_view filepath, FileMode fileMode, 
		FileAccess fileAccess, FileShare fileShare);

	static TPtr<FileStream> create(std::string_view filepath, FileMode fileMode, 
		FileAccess fileAccess);


private:

	std::string _filepath;
	FileMode _fileMode;
	FileAccess _fileAccess;
	FileShare _fileShare;
	
	void* _hFile;
	std::unique_ptr<char[]> _buffer;
	int _capacity;
	long _appendStart { -1 };  //追加模式下，不允许修改已经存在的内容，这里记录的是原始位置。
	int _readPos{ 0 };
	int _readLen{ 0 };
	int _writePos{ 0 };

};
