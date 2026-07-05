#include "FileStream.h"
#include<Windows.h>
#include<format>
#include<iostream>
#include"Encoding.h"
#include"WinUtils.h"
#include<print>

using namespace std;

constexpr int defaultBufferSize = 4096;  //默认设置为4k
//检查返回值如果失败返回错误
#define CHECK_RESULT(result) if (!result) { return std::unexpected{ result.error() }; }

FileStream::FileStream(const std::string& path, FileMode fileMode, FileAccess fileAccess, FileShare fileShare, int bufferSize)
	: hFile(INVALID_HANDLE_VALUE), _capacity(bufferSize < 0 ? defaultBufferSize : bufferSize)
{
	this->_readable = true;
	this->_writeable = true;
	this->_seekable = true;

	if (path.empty())
	{
		//throw invalid_argument("文件字符串不能为空");
	}

	this->_buffer = make_unique<char[]>(this->_capacity);
	this->_writePos = 0;
	this->_readPos = 0;

	SetErrorMode(1); //系统不弹窗，将错误发送给调用进程


	auto result = this->init(path, fileAccess, fileShare, fileMode);
	if (!result)
	{
		println("{}", result.error());
		throw runtime_error(result.error());
	}
}

FileStream::FileStream(const std::string& path, FileMode fileMode, FileAccess fileAccess)
	:FileStream(path, fileMode, fileAccess
		, FileShare::Read, defaultBufferSize)
{}

FileStream::FileStream(const string& path, FileMode fileMode)
	:FileStream(path, fileMode
		, (fileMode == FileMode::Append ? FileAccess::Write : FileAccess::ReadWrite)
		, FileShare::Read, defaultBufferSize)
{}

FileStream::FileStream(FileStream&& other) noexcept
	: Stream(std::move(static_cast<Stream&>(other))), hFile(other.hFile), _buffer(move(other._buffer)), _capacity(other._capacity),
	_readPos(other._readPos), _readLen(other._readLen), _writePos(other._writePos)
{
	other.hFile = INVALID_HANDLE_VALUE;
	other._position = 0;
	other._readable = false;
	other._writeable = false;
	other._seekable = false;
}

//移动赋值运算符，先释放自身内容，再将other的内容移动过来，最后释放other的内容
FileStream& FileStream::operator=(FileStream&& other) noexcept
{
	if (this != &other)
	{
		auto result = this->close(); //不处理返回值
		Stream::operator=(static_cast<Stream&&>(other));
		this->hFile = other.hFile;
		this->_buffer = move(other._buffer);
		this->_capacity = other._capacity;
		this->_readPos = other._readPos;
		this->_readLen = other._readLen;
		this->_writePos = other._writePos;

		other.hFile = INVALID_HANDLE_VALUE;
		other._position = 0;
		other._readable = false;
		other._writeable = false;
		other._seekable = false;
	}
	return *this;
}

FileStream::~FileStream()
{

	this->_readable = false;
	this->_writeable = false;
	this->_seekable = false;
	auto result = this->close(); //不处理返回值
}

std::expected<void, std::string> FileStream::close()
{
	if (this->hFile != INVALID_HANDLE_VALUE)
	{
		auto result = this->flush(true); CHECK_RESULT(result);
		auto ret = CloseHandle(this->hFile);
		this->hFile = INVALID_HANDLE_VALUE;
		if(ret == FALSE)
		{
			return std::unexpected(WinUtils::getError("CloseHandle"));
		}	
	}

	return {};
}

std::expected<void, std::string> FileStream::setLength(long value)
{
	if (this->hFile == INVALID_HANDLE_VALUE)
	{
		return std::unexpected("文件未打开!!");
	}

	if (this->_writePos > 0)
	{
		auto result = this->flushWrite();
		if (!result)
		{
			return std::unexpected(result.error());
		}
	}
	else if (this->_readPos < this->_readLen)
	{
		auto result = this->flushRead();
		if (!result)
		{
			return std::unexpected(result.error());
		}
	}

	this->_writePos = 0;
	this->_readPos = 0;

	if (this->_appendStart != -1 && value < this->_appendStart)
	{
		return std::unexpected("追加模式下不允许修改现有内容");
	}

	//真正的调整文件长度内容  setLengthCore
	auto position = this->_position;
	LARGE_INTEGER setPos;
	LARGE_INTEGER newPos;
	setPos.QuadPart = value;

	BOOL ret;
	if (this->_position != value)
	{
		ret = SetFilePointerEx(this->hFile, setPos, &newPos, SEEK_SET);
		if (ret == FALSE)
		{
			return std::unexpected(WinUtils::getError("SetFilePointerEx"));
		}
	}

	//设置之后就要进行截断。
	ret = SetEndOfFile(this->hFile);  //将文件物理末尾位置设置为当前文件指针所在的位置，截断或者扩展， 文件指针在末尾之后，拉长
	if (ret == FALSE)
	{
		return std::unexpected(WinUtils::getError("SetEndOfFile"));
	}
	//要求hFile具有 write权限，如果文件被影射了，必须先解除映射再调用。  截断之后文件尺寸变小。 这个行为不改变文件指针。

	if (position != value)
	{
		if (position < value)
		{
			setPos.QuadPart = position;
			ret = SetFilePointerEx(this->hFile, setPos, &newPos, SEEK_SET); //原始位置比长度小，就从头开始恢复。
			if (ret == FALSE)
			{
				return std::unexpected(WinUtils::getError("SetFilePointerEx"));
			}
		}
		else if (position > value)
		{
			setPos.QuadPart = 0;
			ret = SetFilePointerEx(this->hFile, setPos, &newPos, SEEK_END);//原始位置比长度大，文件已经截断，需要重新定位指针到文件末尾。
			if (ret == FALSE)
			{
				return std::unexpected(WinUtils::getError("SetFilePointerEx"));
			}
		}
	}
	
	return {};
}

std::expected<long, std::string> FileStream::getLength()
{

	if (this->hFile == INVALID_HANDLE_VALUE)
	{
		return std::unexpected("文件未打开!!");
	}

	LARGE_INTEGER size;
	//获取文件尺寸
	auto result = GetFileSizeEx(this->hFile, &size);
	if (result == FALSE)
	{
		throw invalid_argument("GetFileSizeEx");
	}

	long length = this->_writePos + this->_position;
	if (this->_writePos > 0 && length > size.QuadPart)
	{
		return length;
	}

	return static_cast<long>(size.QuadPart);
}

std::expected<long, std::string> FileStream::getPosition()
{
	if (this->hFile == INVALID_HANDLE_VALUE)
	{
		return std::unexpected("文件未打开!!!");
	}
	
	return this->_position + (this->_readPos - this->_readLen + this->_writePos);
}

std::expected<void, std::string> FileStream::setPosition(long value)
{
	if (this->hFile == INVALID_HANDLE_VALUE)
	{
		return std::unexpected("文件未打开!!!");
	}

	if (value < 0)
	{
		return std::unexpected("文件指针必须为正数");
	}

	//这个设置是绝对的
	if (this->_writePos > 0)
	{
		auto result = this->flushWrite();
		if (!result)
		{
			return std::unexpected(result.error());
		}
	}
	this->_writePos = 0;
	this->_readPos = 0;

	{
		auto result = this->seek(value, SeekOrigin::Begin);
		if (!result)
		{
			return std::unexpected(result.error());
		}
	}


	return {};
}

std::expected<void, std::string> FileStream::flush()
{
	return this->flush(false);
}

std::expected<void, std::string> FileStream::flush(bool flushToDisk)
{
	if (this->hFile == INVALID_HANDLE_VALUE)
	{
		return std::unexpected("文件未打开!!!");
	}
	if (_writePos > 0)
	{
		auto result = this->flushWrite();
		if (!result)
		{
			return std::unexpected(result.error());
		}
	}
	else if (_readPos < _readLen)
	{
		auto result = this->flushRead();
		if (!result)
		{
			return std::unexpected(result.error());
		}
	}

	if (flushToDisk)
	{
		auto ret = FlushFileBuffers(this->hFile);
		if (ret == FALSE)
		{
			return std::unexpected(WinUtils::getError("FlushFileBuffers"));
		}
	}

	return {};
}

std::expected<long, std::string> FileStream::seek(long offset, SeekOrigin origin)
{
	if (this->hFile == INVALID_HANDLE_VALUE)
	{
		return std::unexpected("文件未打开!!!");
	}

	if (this->canSeek() == false)
	{
		return std::unexpected("文件指针不能移动");
	}

	if (origin < SeekOrigin::Begin || origin > SeekOrigin::End)
	{
		return std::unexpected("不合法的seekOrigin");
	}

	//如果缓冲区中有内容，先将缓冲区内容写入文件中
	if (this->_writePos > 0)
	{
		auto result = this->flushWrite();
		if (!result)
		{
			return result;
		}
	}
	else if (origin == SeekOrigin::Current)
	{
		offset -= this->_readLen - this->_readPos;  //去除缓冲区的是真正要移动的
	}

	long num = this->_position + (this->_readPos - this->_readLen); //修正位置
	long num2 = 0;

	{
		auto result = this->seekCore(offset, origin);  //文件实际的指针位置。
		if (!result)
		{
			return result;
		}
		num2 = result.value();  //调整后的实际位置
	}

	if (this->_appendStart != -1 && num < this->_appendStart)
	{
		auto result = this->seekCore(num, SeekOrigin::Begin);   //修正到实际位置。
		if (!result)
		{
			return result;
		}
		return std::unexpected("追加模式下，不允许修改已经存在的内容");
	}

	if (this->_readLen > 0)
	{
		long k = num2 - num;
		if (k > -this->_readPos && k < this->_readLen - this->_readPos)
		{
			std::copy_n(this->_buffer.get() + this->_readPos + k, 
				this->_readLen - this->_readPos - k,  this->_buffer.get());
			this->_readLen -= _readPos;
			this->_readPos = 0;
			if (this->_readLen > 0)
			{
				auto result = this->seekCore(this->_readLen, SeekOrigin::Current);
				if (!result)
				{
					return result;
				}
			}
		}
		else
		{
			this->_readPos = 0;
			this->_readLen = 0;
		}
	}
	return num2;
}

//读取文件
std::expected<long, std::string> FileStream::basic_read(char* array, int size, int offset, int count)
{
	if (this->hFile == INVALID_HANDLE_VALUE)
	{
		return std::unexpected("文件未打开!!!");
	}

	if (this->canRead() == false)
	{
		return std::unexpected("文件不支持读取");
	}

	//如果之前是写入场景，先将缓冲区内容写入文件中，再进行读取
	if (this->_writePos > 0)
	{
		auto result = this->flushWrite();
		if (!result)
		{
			return result;
		}
	}
	DWORD totalSize = 0;  //总读取数量
	auto remaindSize = this->_readLen - this->_readPos;  //已读缓冲区剩余的内容

	auto copySize = 0;  //拷贝的数据内容

	if (remaindSize > 0)
	{
		copySize = (std::min)(count, remaindSize);  //要拷贝的大小，不能超过count，也不能超过缓冲区剩余的内容
		std::copy_n(this->_buffer.get() + this->_readPos, copySize, array + offset);
		this->_readPos += copySize;   //移动指针
		totalSize += copySize;
		offset += copySize;
		count -= copySize;

		if( count == 0)
		{
			return totalSize;  //读完了直接返回
		}
	}
	
	
	if (count > this->_capacity) //剩余没拷贝的超过了内置缓冲区大小,直接读到请求缓冲区
	{
		auto result = this->readCore(array, size, offset, count);
		if (!result)
		{
			return result;
		}

		this->_readPos = 0;
		this->_readLen = 0;
		totalSize += result.value();
		return totalSize;
	}
	else
	{
		auto result = this->readCore(this->_buffer.get(), this->_capacity, 0, this->_capacity);
		if (!result)
		{
			return result;
		}
		if (copySize == 0)
		{
			copySize = count;
		}

		if (copySize > result.value()) //如果读取数量不够，就只拷贝实际读取的数量
		{
			copySize = result.value();
		}
		if (copySize > 0)
		{
			std::copy_n(this->_buffer.get(), copySize, array + offset);
			this->_readPos = copySize;
			this->_readLen = result.value();
			totalSize += copySize;
		}
		return totalSize;
	}
}

//写入文件
std::expected<long, std::string>  FileStream::basic_write(const char* array, int size, int offset, int count)
{
	if (this->hFile == INVALID_HANDLE_VALUE)
	{
		return std::unexpected("文件未打开!!!");
	}

	if (this->canWrite() == false)
	{
		return std::unexpected("文件不支持写入");
	}

	if (this->_writePos == 0)  //写指针为0，之前可能是读场景，先清理读状态
	{
		//如果之前是读场景
		if (this->_readPos < this->_readLen)
		{
			auto result = flushRead();
			if(!result)
			{
				return std::unexpected(result.error());
			}
		}
		this->_readPos = 0;
		this->_readLen = 0;
	}

	auto remaindSize = this->_capacity - this->_writePos;  //剩余可写空间

	if (remaindSize >= count)  //剩余缓冲区有空间，写入缓冲区
	{
		std::copy_n(array + offset, count, this->_buffer.get() + this->_writePos);
		this->_writePos += count;
		return count;
	}
	else
	{
		//如果缓冲区中有内容，先将缓冲区内容写入文件中
		if (this->_writePos > 0)
		{
			auto result = this->writeCore(this->_buffer.get(), this->_capacity, 0, this->_writePos);
			if (!result)
			{
				return result;
			}

			this->_writePos = 0;
			//这里写的是历史数据，不计入本次写入数量
		}

		//将外部数据写入文件
		{
			auto result = this->writeCore(array, size, offset, count);
			if (!result)
			{
				return result;
			}
			return result.value();
		}

	}
}


//根据缓冲区可能存在内容，刷新文件指针
std::expected<void, std::string> FileStream::flushRead()
{
	auto offset = this->_readPos - _readLen;   //读取场景下，没有读完但在缓冲区的内容,恢复文件指针位置
	if (offset != 0)
	{
		auto result = this->seek(offset, SeekOrigin::Current);
		if (!result)
		{
			return std::unexpected(result.error());
		}
		this->_readPos = 0;
		this->_writePos = 0;
	}

	return {};
}

//将缓冲区内容写到文件中
std::expected<long, std::string> FileStream::flushWrite()
{

	if (this->_writePos == 0)
	{
		return 0;
	}
	auto result = this->writeCore(this->_buffer.get(), this->_capacity, 0, this->_writePos);
	if (!result)
	{
		return result;
	}

	this->_writePos = 0;
	this->_readPos = 0;

	return result.value();
}

std::expected<long, std::string> FileStream::writeCore(const char* data, int size, int offset, int count)
{
	if(size < 0 || offset + count > size)
	{
		return std::unexpected("无效的缓冲区大小或者偏移量");
	}
	DWORD wSize = 0;
	auto ret = WriteFile(this->hFile, data + offset, count, &wSize, NULL);
	if (ret == FALSE)
	{
		return std::unexpected(WinUtils::getError("WriteFile"));
	}
	this->_position += wSize;
	return wSize;
}

std::expected<long, std::string> FileStream::readCore(char* data, int size, int offset, int count)
{
	if(size < 0 || offset + count > size)
	{
		return std::unexpected("无效的缓冲区大小或者偏移量");
	}
	DWORD rSize = 0;
	auto ret = ReadFile(this->hFile, data + offset, count, &rSize, NULL);
	if (ret == FALSE)
	{
		return std::unexpected(WinUtils::getError("ReadFile"));
		//读取文件失败
	}
	this->_position += rSize;
	return rSize;
}

std::expected<long, std::string> FileStream::seekCore(long offset, SeekOrigin origin)
{
	LARGE_INTEGER setPos;  //要设置的位置
	LARGE_INTEGER newPos;	//新返回的位置
	setPos.QuadPart = offset;
	BOOL ret = SetFilePointerEx(this->hFile, setPos, &newPos, static_cast<DWORD>(origin));
	if (ret == FALSE)
	{
		return std::unexpected(WinUtils::getError("SetFilePointerEx"));
	}
	this->_position = static_cast<long>(newPos.QuadPart);  //刷新当前位置
	return this->_position;
}



std::expected<void, std::string> FileStream::init(const string& path, FileAccess fileAccess, FileShare fileShare, FileMode fileMode)
{
	if (path.empty())
	{
		return std::unexpected("文件路径不能为空");
	}

	DWORD dwAccess = 0;
	if (fileAccess == FileAccess::ReadWrite)
	{
		dwAccess = GENERIC_READ | GENERIC_WRITE;
		this->_readable = true;
		this->_writeable = true;
	}
	else if (fileAccess == FileAccess::Read)
	{
		dwAccess = GENERIC_READ;
		this->_readable = true;
		this->_writeable = false;
	}
	else if (fileAccess == FileAccess::Write)
	{
		dwAccess = GENERIC_WRITE;
		this->_writeable = true;
		this->_readable = false;
	}

	DWORD dwShareMode = 0;
	if (fileShare == FileShare::ReadWrite)
	{
		dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
	}
	else if (fileShare == FileShare::Read)
	{
		dwShareMode = FILE_SHARE_READ;
	}
	else if (fileShare == FileShare::Write)
	{
		dwShareMode = FILE_SHARE_WRITE;
	}


	DWORD dwFileMode = 0;

	if (fileMode == FileMode::Append)
	{
		if (fileAccess != FileAccess::Write)
		{
			return std::unexpected("追加模式只能与写入权限一起使用");
		}
		else
		{
			dwFileMode = OPEN_ALWAYS;
		}
	}
	else
	{
		dwFileMode = static_cast<DWORD>(fileMode);
		if (dwFileMode < CREATE_NEW || dwFileMode > TRUNCATE_EXISTING)
		{
			return std::unexpected("不合法的FileMode");
		}
	}

	wstring wpath = Encoding::GBK2UTF16(path);
	this->hFile = CreateFileW(wpath.c_str(),
		dwAccess,   //文件访问权限 读写  如果是0就是不请求只是用来查询
		dwShareMode,    //指定文件共享模式 允许其他进程读取这个文件
		NULL,				//安全属性  NULL表示默认安全描述符，文件句柄不能被子进程继承
		dwFileMode,			//指定如何创建或打开文件  CREATE_NEW表示创建新文件，如果文件已存在则失败
		0,				//文件属性和标志  FILE_ATTRIBUTE_NORMAL表示普通文件，可以在这里设置FILE_FLAG_OVERLAPPED等标志来控制文件的行为
		NULL);				//模板文件句柄 0表示不使用模板文件

	if (this->hFile == INVALID_HANDLE_VALUE)
	{
		return std::unexpected(WinUtils::getError("CreateFileW"));
	}


	auto fileType = GetFileType(this->hFile);

	if (fileType != FILE_TYPE_DISK)
	{
		CloseHandle(this->hFile);
		return std::unexpected("仅支持磁盘文件");
	}


	if (fileMode == FileMode::Append)
	{
		auto result = this->seekCore(0, SeekOrigin::End);  //追加模式下移动到末尾
		if (!result)
		{
			return std::unexpected(result.error());
		}
		this->_appendStart = result.value();
	}
	else
	{
		this->_appendStart = -1;//非追加模式，设置为-1，表示不限制修改位置
	}


	return {};
}
