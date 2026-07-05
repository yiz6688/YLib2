#include"Stream.h"

Stream::Stream()
	: _position(0), _readable(false), _writeable(false), _seekable(false)
{

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

std::expected<long, std::string> Stream::read(char* buffer, int size, int offset, int count)
{
	return this->basic_read(buffer, size, offset, count);
}

std::expected<long, std::string> Stream::read(char* buffer, int size)
{
	return this->basic_read(buffer, size, 0, size);
}

std::expected<long, std::string> Stream::read(std::vector<char>& vec)
{
	auto size = vec.size();
	return this->basic_read(vec.data(), static_cast<int>(size), 0, static_cast<int>(size));
}

std::expected<long, std::string> Stream::write(const std::string& str)
{
	return this->write(str.data(), static_cast<int>(str.size()));
}

std::expected<long, std::string> Stream::write(const std::vector<char>& vec)
{
	return this->write(vec.data(), static_cast<int>(vec.size()));
}

std::expected<long, std::string> Stream::write(const char* data, int size, int offset, int count)
{
	return this->basic_write(data, size, offset, count);
}

std::expected<long, std::string> Stream::write(const char* data, int size)
{
	return this->basic_write(data, size, 0, size);
}

void Stream::copyTo(Stream& stream)
{
	this->InternalCopyTo(stream, 4096);
}

void Stream::InternalCopyTo(Stream& stream, int bufferSize)
{
	{
		char* array = new char[bufferSize];
		int count;
		while (true)
		{
			auto result = this->read(array, bufferSize);
			if (!result)
			{
				delete[] array;
				return;
			}
			count = result.value();
			if (count == 0)
			{
				delete[] array;
				return;
			}
			auto writeResult = stream.write(array, count);
			if (!writeResult)
			{
				delete[] array;
				return;
			}
		}
		//while ((count = this->read(array, bufferSize)) != 0)
		//{
		//	stream.write(array, count);
		//}
	}
}


