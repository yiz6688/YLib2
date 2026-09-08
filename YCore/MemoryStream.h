#pragma once
#include"Stream.h"
#include<memory>

class MemoryStream : public Stream
{
public:
	MemoryStream();

	MemoryStream(int size);

	MemoryStream(char* data, int dataLen, int offset, int count, bool visiable = false);

	~MemoryStream();

	long getLength() override;

	void setLength(long value) override;

	long getPosition() override;

	void setPosition(long position) override;

	long getCapacity();

	long setCapacity(long value);

	void flush() override;

	long seek(long offset, SeekOrigin origin) override;

protected:
	std::expected<void, std::string> inner_close() override;

	long basic_read(char* buffer, int size, int offset, int count) override;

	long basic_write(const char* data, int size, int offset, int count) override;

private:
	//确保容量 >= value(value 为绝对偏移)，返回值:true 表示新分配了数组,false 表示无需分配
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

	//流是否处于打开状态
	bool _isOpen{ true };

};
