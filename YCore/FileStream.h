#pragma once
#include"Stream.h"
#include<string>
#include<memory>


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


class FileStream : public Stream
{
public:

	//FileStream(const char* path, int mode);

	FileStream(const std::string& path, FileMode fileMode, FileAccess fileAccess, FileShare fileShare, int bufferSize);

	FileStream(const std::string& path, FileMode fileMode, FileAccess fileAccess);

	FileStream(const std::string& path, FileMode fileMode);

	FileStream(FileStream&& other) noexcept;

	FileStream& operator=(FileStream&& other) noexcept;


	~FileStream();

	std::expected<void, std::string> close() override;

	std::expected<void, std::string> setLength(long value) override;

	std::expected<long, std::string> getLength() override;

	std::expected<long, std::string> getPosition() override;

	std::expected<void, std::string> setPosition(long value) override;

	std::expected<void, std::string> flush() override;

	std::expected<void, std::string> flush(bool flushToDisk);

	std::expected<long, std::string>  seek(long offset, SeekOrigin origin) override;

protected:
	std::expected<long, std::string> basic_read(char* data, int size, int offset, int count) override;

	std::expected<long, std::string> basic_write(const  char* data, int size, int offset, int count) override;




private:
	std::expected<void, std::string> flushRead();

	std::expected<long, std::string> flushWrite();

	std::expected<long, std::string> writeCore(const char* data, int size, int offset, int count);

	std::expected<long, std::string> readCore(char* data, int size, int offset, int count);

	std::expected<long, std::string> seekCore(long offset, SeekOrigin origin);

public:
	std::expected<void, std::string> init(const std::string& path, FileAccess fileAccess, FileShare fileShare, FileMode fileMode);

public:
	static std::expected<FileStream, std::string> create(std::string_view filepath, FileAccess fileAccess, FileShare fileShare, FileMode fileMode);

	static std::expected<FileStream, std::string> create(std::string_view filepath, FileAccess fileAccess, FileMode fileMode);


private:
	void* hFile;
	std::unique_ptr<char[]> _buffer;
	int _capacity;
	long _appendStart;  //追加模式下，不允许修改已经存在的内容，这里记录的是原始位置。
	int _readPos{ 0 };
	int _readLen{ 0 };
	int _writePos{ 0 };

};