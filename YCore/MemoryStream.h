#pragma once
#include"Stream.h"
#include<memory>

class MemoryStream : public Stream
{
public:
	MemoryStream();

	MemoryStream(int size);

	MemoryStream(char* data, int dataLen, int offset, int count, bool visiable);

	~MemoryStream();

	std::expected<long, std::string> getLength() override;

	std::expected<void, std::string> setLength(long value) override;

	std::expected<long, std::string> getPosition() override;

	std::expected<void, std::string> setPosition(long position) override;

	long getCapacity();

	std::expected<long, std::string> setCapacity(long value);

	std::expected<void, std::string> flush() override;

	std::expected<long, std::string>  seek(long offset, SeekOrigin origin) override;

	std::expected<void, std::string> close() override;

protected:
	std::expected<long, std::string> basic_read(char* buffer, int size, int offset, int count) override;

	std::expected<long, std::string> basic_write(const char* data, int size, int offset, int count) override;

private:
	bool ensureCapacity(long value);


private:
	std::unique_ptr<char[]> _buffer;

	char* _ptr;
	//当前的位置,接下来在后面读写
	//容量大小
	long _capacity;
	//有效长度
	long _length;
	//初始位置，只有以空间构建的才有意义，其他的均为0
	long _origin;

	//是否可获站，只有以空间构建的可以扩展，其他的均不可扩展
	bool _expandable;

};