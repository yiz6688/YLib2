#pragma once
#include<atomic>
#include<memory>
#include<span>
/// <summary>
/// 环形缓冲区
/// 1、支持无锁读写
/// 2、不会读取不存在的数据、不会覆写已存在的数据
/// 3、任一时刻，可读空间和可写空间都小于等于缓冲区总和
/// 4、
/// </summary>


/**
*环形缓冲区类
* 1、支持单生产者单消费者 无锁读写, 消费者的速度要大于生产者
* 2、任一时刻，可读空间+可写空间等于总缓冲区大小
* 3、当写快读慢的时候，进行覆盖
*/
class RingBuffer
{
public:
	//构造指定大小的缓冲区
	RingBuffer(int bufferSize);
	//使用外部空间包装环形缓冲区
	RingBuffer(char* buffer, int size);

	~RingBuffer();

	RingBuffer(RingBuffer&& value) noexcept;
	RingBuffer& operator=(RingBuffer&& value) noexcept;


	//从环形缓冲区读取数据，数据读取完毕后更新对应坐标，不会读取不存在的数据,检查读入的长度和采样点字节对齐
	int read(char* buffer, int size);

	int read(char* buffer, int offset, int size);

	//写入环形缓冲区，数据写入完成后再更新对应坐标，写入时不会覆盖已存在数据，检查写入的长度和采样点字节对齐
	int write(const char* data, int datalen);
	
	int write(const char* data, int offset, int size);

	//从另一个环形缓冲区读
	int readFrom(RingBuffer& ring, int size);

	//写入到另一个环形缓冲区
	int writeTo(RingBuffer& ring, int size);

	//清空
	void reset()
	{
		this->write_pos.store(0, std::memory_order_release);
		this->used_count.store(0, std::memory_order_release);
	}


	size_t getReadableBytes()
	{
		auto value = this->used_count.load(std::memory_order_acquire);
		return value;
	}

	size_t getWriteableBytes()
	{
		return this->_capacity - this->getReadableBytes();
	}

	size_t getCapacity()
	{
		return this->_capacity;
	}

private:
	size_t getReadPos() //获取读指针
	{
		auto writePos = this->write_pos.load(std::memory_order_acquire);
		auto usedCount = this->used_count.load(std::memory_order_acquire);

		auto readPos = writePos - usedCount;
		if (readPos < 0)
		{
			readPos += this->_capacity;
		}

		return readPos;
	}

private:

	//容量大小
	long _capacity;
	//内部分配的空间
	std::unique_ptr<char[]> _buffer;
	//缓冲区的引用
	char* _ptr{ nullptr };

	// 写指针位置
	alignas(64) std::atomic<size_t> write_pos{ 0 }; 
	// 已使用的空间数量
	alignas(64) std::atomic<size_t> used_count{ 0 }; 

};