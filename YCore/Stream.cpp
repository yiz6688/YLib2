#include"Stream.h"

Stream::Stream()
	: _position(0), _readable(false), _writeable(false), _seekable(false)
{

}

//正常关闭: 内部转调 inner_close, 失败抛出异常
void Stream::close()
{
	auto r = this->inner_close();
	if (!r)
	{
		throw std::runtime_error(r.error());
	}
}

Stream::Stream(Stream&& other) noexcept
	: _position(other._position), _readable(other._readable),
	_writeable(other._writeable), _seekable(other._seekable)
{
	other._position = 0;
	other._readable = false;
	other._writeable = false;
	other._seekable = false;
}

long Stream::read(char* buffer, int size, int offset, int count)
{
	return this->basic_read(buffer, size, offset, count);
}

long Stream::read(char* buffer, int size)
{
	return this->basic_read(buffer, size, 0, size);
}

long Stream::read(std::vector<char>& vec)
{
	auto size = vec.size();
	return this->basic_read(vec.data(), static_cast<int>(size), 0, static_cast<int>(size));
}

long Stream::write(const std::string& str)
{
	return this->write(str.data(), static_cast<int>(str.size()));
}

long Stream::write(const std::vector<char>& vec)
{
	return this->write(vec.data(), static_cast<int>(vec.size()));
}

long Stream::write(const char* data, int size, int offset, int count)
{
	return this->basic_write(data, size, offset, count);
}

long Stream::write(const char* data, int size)
{
	return this->basic_write(data, size, 0, size);
}

void Stream::copyTo(Stream& stream)
{
	this->InternalCopyTo(stream, 4096);
}

void Stream::InternalCopyTo(Stream& stream, int bufferSize)
{
	if (bufferSize <= 0)
	{
		return;
	}
	//使用 vector 管理缓冲区，避免手动 new/delete 可能引发的内存泄漏
	std::vector<char> array(static_cast<size_t>(bufferSize));
	while (true)
	{
		long count = this->read(array.data(), bufferSize);  //失败抛出异常
		if (count == 0)
		{
			return;
		}
		stream.write(array.data(), static_cast<int>(count));
	}
}
