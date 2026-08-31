#pragma once
#include<atomic>
#include<vector>
#include<span>
#include<algorithm>
#include<bit>
#include<limits>
#include<cassert>
#include<stdexcept>
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
template<typename T=char>
class RingBuffer2
{

private:
	using TYPE1 = unsigned;	

public:
	//构造指定大小的缓冲区
	RingBuffer2(unsigned bufferSize, unsigned int gap = sizeof(T))
        :_capacity{ std::bit_ceil(bufferSize) }, _mask{ _capacity - 1 }, _gap{ gap }, 
	_buffer(_mask + 1), _ptr{ _buffer.data() }
    {
        if(this->_gap != sizeof(T))
        {
            if(this->_gap % sizeof(T) != 0)
            {
                throw std::runtime_error("gap 必须是 当前类的整倍数");
            }
        }
        this->_cap_aligned = (this->_capacity / this->_gap) * this->_gap;
        this->_max_size = this->_cap_aligned - this->_gap;
    }
	//使用外部空间包装环形缓冲区
	RingBuffer2(char* buffer, unsigned size)
    	:_capacity{ size }, _mask{ size - 1 }, _gap{ sizeof(T) }, _buffer(), _ptr{ buffer }
    {
        assert(size>0 &&(size & (size - 1)) == 0); // size must be a power of 2
        assert(buffer != nullptr);

        this->_cap_aligned = (this->_capacity / this->_gap) * this->_gap;
        this->_max_size = this->_cap_aligned - this->_gap;

    }

	~RingBuffer2()
    {
        this->_ptr = nullptr;
    }

	RingBuffer2(RingBuffer2&& value) noexcept
    {
        this->_capacity = value._capacity;
        this->_mask = value._mask;
        this->_gap = value._gap;
        this->_cap_aligned = value._cap_aligned;
        this->_max_size = value._max_size;
        this->_buffer = std::move(value._buffer);
        this->_ptr = value._ptr;
        this->write_pos.store(value.write_pos.load());
        this->read_pos.store(value.read_pos.load());
        
        this->write_lock_len = value.write_lock_len;
        this->read_lock_len = value.read_lock_len;
    }

	RingBuffer2& operator=(RingBuffer2&& value) noexcept
    {
        if (&value == this)
        {
            return *this;
        }
        this->_capacity = value._capacity;
        this->_mask = value._mask;
        this->_gap = value._gap;
        this->_cap_aligned = value._cap_aligned;
        this->_max_size = value._max_size;
        this->_buffer = std::move(value._buffer);
        this->_ptr = value._ptr;
        this->write_pos.store(value.write_pos.load());
        this->read_pos.store(value.read_pos.load());
        this->write_lock_len = value.write_lock_len;
        this->read_lock_len = value.read_lock_len;
        return *this;
    }


	//从环形缓冲区读取数据，数据读取完毕后更新对应坐标，不会读取不存在的数据,检查读入的长度和采样点字节对齐
	int read(T* buffer, int size)
    {
        return this->read(buffer, 0, size);
    }

	int read(T* buffer, int offset, int size)
    {

        if(this->read_lock_len != 0 || size == 0)
        {
            return 0;
        }

        auto wpos = this->write_pos.load(std::memory_order_acquire);
        auto rpos = this->read_pos.load(std::memory_order_relaxed);

        

        int readPos = rpos & this->_mask;

        auto readableBytes = this->calcReadableBytes(wpos, rpos); //计算可读空间
        if(readableBytes == 0)
        {
            return 0;
        }
        if (readableBytes > size)
        {
            readableBytes = size;  //修正可读数量
        }


        auto size1 = this->_cap_aligned - readPos;  //读指针到尾部的空间
        if (size1 > readableBytes)
        {
            size1 = readableBytes;
        }else
        {
            rpos += (this->_capacity - this->_cap_aligned); //修正位置
        }

        std::copy_n(this->_ptr + readPos, size1, buffer + offset);

        auto size2 = readableBytes - size1;
        if (size2 > 0)
        {
            offset += size1;
            std::copy_n(this->_ptr, size2, buffer + offset);
        }

        rpos += readableBytes;
        this->read_pos.store(rpos, std::memory_order_release);//或者更新已用空间容量。


        return readableBytes;
    }


	//写入环形缓冲区，数据写入完成后再更新对应坐标，写入时不会覆盖已存在数据，检查写入的长度和采样点字节对齐
	int write(const char* data, int size)
    {
        return this->write(data, 0, size);
    }
	
	int write(const char* data, int offset, int size)
    {
        if(this->write_lock_len != 0 || size == 0)
        {
            return 0;
        }

        auto wpos = this->write_pos.load(std::memory_order_relaxed);
        auto rpos = this->read_pos.load(std::memory_order_acquire);
        

        auto writePos = wpos & this->_mask;

        auto writeableBytes = this->calcWriteableBytes(wpos, rpos); //计算可写空间
        if(writeableBytes == 0)
        {
            return 0;
        }

        if (writeableBytes > size)
        {
            writeableBytes = size;
        }

        auto size1 = this->_cap_aligned - writePos;  //读指针到尾部的空间
        if (size1 > writeableBytes)
        {
            size1 = writeableBytes;
        }else
        {
            wpos += (this->_capacity - this->_cap_aligned);
        }
        
        std::copy_n(data + offset, size1, this->_ptr + writePos);

        auto size2 = writeableBytes - size1;
        if (size2 > 0)
        {
            offset += size1;
            std::copy_n(data + offset, size2, this->_ptr); //从头部开始写
        }

        wpos += writeableBytes; //修正写指针
        this->write_pos.store(wpos, std::memory_order::release);

        return writeableBytes;
    }

	//从另一个环形缓冲区读
	int readFrom(RingBuffer2& ring, int size)
    {
        int nCapacity = ring.getCapacity();
        //int nReadPos = 	ring.getReadPos();

        int nReadPos = 0;// 待验证修复
        int nReadSize = ring.getReadableBytes();
        int nEndPos = nReadPos + nReadSize;

        int size1 = nCapacity - nReadPos;
        int size2 = 0;

        int ReadSize = 0;

        if (size1 > nReadPos)
        {
            size1 = nReadPos;
        }
        else
        {
            size2 = nReadPos - size1;
        }
        char* ptr1 = ring._ptr + nReadPos;
        ReadSize = this->write(ptr1, size1);
        
        if (size2 > 0)
        {
            size1 = this->write(ring._ptr, size2);
            ReadSize += size1;
        }


        return ReadSize;
    }

	//写入到另一个环形缓冲区
	int writeTo(RingBuffer2& ring, int size)
    {
        return ring.readFrom(*this, size);
    }


	std::span<T> getWriteBuffer(TYPE1 size)
    {
        auto wpos = this->write_pos.load(std::memory_order_relaxed);
        auto writePos = wpos & this->_mask;  //相当于求余
        auto wptr = this->_ptr + writePos;  //写指针起始位置。

        if(size == 0 || this->write_lock_len != 0)
        {
            return std::span<T>(wptr, 0);
        }

        auto rpos = this->read_pos.load(std::memory_order_acquire);

        auto writeableBytes = this->calcWriteableBytes(wpos, rpos); //计算剩余可写空间
        if(writeableBytes > size)
        {
            writeableBytes = size;
        }

        auto tailBytes = this->_cap_aligned - writePos;  //写指针到尾部的空间
        if(tailBytes > writeableBytes) //说明没有回绕
        {
            this->write_lock_len = writeableBytes;
        }else
        {
            this->write_lock_len = tailBytes;
        }

        this->write_pos.store(wpos, std::memory_order_release);
        return std::span<T>(wptr, this->write_lock_len);
    }

	int releaseWriteBuffer(TYPE1 size)
    {
        if(size == 0 || this->write_lock_len == 0)
        {
            return 0;
        }else if(size == std::numeric_limits<TYPE1>::max())
        {
            size = this->write_lock_len;
        }

        if(size > this->write_lock_len)
        {
            size = this->write_lock_len;
        }

        //这里必定不会产生回绕
        auto wpos = this->write_pos.load(std::memory_order_relaxed);
        //auto rpos = this->read_pos.load(std::memory_order_acquire);

        auto releaseSize = this->write_lock_len > size ? size : this->write_lock_len;

        wpos += releaseSize;
        auto writePos = wpos & this->_mask;
        if(writePos  == this->_cap_aligned)
        {
            wpos += (this->_capacity - this->_cap_aligned);
        }

        this->write_lock_len -= releaseSize;
        this->write_pos.store(wpos, std::memory_order_release);
        return releaseSize;
    }

	int releaseWriteBuffer()
    {
        return this->releaseWriteBuffer(-1);
    }

	std::span<T> getReadBuffer(TYPE1 size)
    {
        auto rpos = this->read_pos.load(std::memory_order_relaxed);

        auto readPos = rpos & this->_mask;  //相当于求余
        auto rptr = this->_ptr + readPos;  //写指针起始位置。

        if(size == 0 || this->read_lock_len != 0)
        {
            return std::span<T>(rptr, 0);
        }
        
        auto wpos = this->write_pos.load(std::memory_order_acquire);

        auto readableBytes = this->calcReadableBytes(wpos, rpos); //计算可读空间
        if(readableBytes > size)
        {
            readableBytes = size;
        }

        auto tailBytes = this->_cap_aligned - readPos;  //写指针到尾部的空间
        if(tailBytes > readableBytes) //说明没有回绕
        {
            this->read_lock_len = readableBytes;
        }else
        {
            this->read_lock_len = tailBytes;
        }

        this->read_pos.store(rpos, std::memory_order_release);
        return std::span<T>(rptr, this->read_lock_len);
    }

	int releaseReadBuffer(TYPE1 size)
    {
        if(size == 0 || this->read_lock_len == 0)
        {
            return 0;
        }else if(size == std::numeric_limits<TYPE1>::max())
        {
            size = this->read_lock_len;
        }
        //这里必定不会产生回绕
        //auto wpos = this->write_pos.load(std::memory_order_acquire);
        auto rpos = this->read_pos.load(std::memory_order_relaxed);

        auto releaseSize = this->read_lock_len > size ? size : this->read_lock_len;
        rpos += releaseSize;
        auto readPos = rpos & this->_mask;
        if(readPos == this->_cap_aligned)
        {
            rpos += (this->_capacity - this->_cap_aligned);
        }
        this->read_lock_len -= releaseSize;

        this->read_pos.store(rpos, std::memory_order_release);
        return releaseSize;
    }

	int releaseReadBuffer()
    {
        return this->releaseReadBuffer(-1);
    }



private:
	TYPE1 calcReadableBytes(TYPE1 wpos, TYPE1 rpos)
	{
		auto bytes = (wpos - rpos) & this->_mask;
		if(this->_cap_aligned != this->_capacity)
		{
			auto writePos = wpos & this->_mask;
			auto readPos = rpos & this->_mask;
			if(writePos < readPos)  //不在一圈，多加了尾巴
			{
				bytes = bytes - (this->_capacity - this->_cap_aligned);  //减去多余的内容
			}
		}
		return bytes;
	}

	TYPE1 calcWriteableBytes(TYPE1 wpos, TYPE1 rpos)
	{
		auto readableBytes = this->calcReadableBytes(wpos, rpos);
		auto bytes = this->_max_size - readableBytes;
		return bytes;
	}

	TYPE1 calcPos1(TYPE1 pos, TYPE1 size)
	{
		pos += size;
		if(pos == this->_cap_aligned)
		{
			pos +=(this->_capacity - this->_cap_aligned);
		}
		return pos;
	}


public:

	//清空
	void reset()
	{
		this->read_pos.store(0, std::memory_order_release);
		this->write_pos.store(0, std::memory_order_release);
		this->write_lock_len = 0;
		this->read_lock_len = 0;
	}



	size_t getReadableBytes()
	{
		auto wpos = this->write_pos.load(std::memory_order_acquire);
		auto rpos = this->read_pos.load(std::memory_order_acquire);
		auto bytes = this->calcReadableBytes(wpos, rpos);
		return bytes;
	}

	size_t getWriteableBytes()
	{
		auto wpos = this->write_pos.load(std::memory_order_acquire);
		auto rpos = this->read_pos.load(std::memory_order_acquire);
		auto bytes = this->calcWriteableBytes(wpos, rpos);
		return bytes;
	}

	size_t getCapacity()
	{
		return this->_capacity;
	}




private:

	//总容量大小
	unsigned _capacity;
	//进行取余使用，是2的幂次方减1
	unsigned _mask;  //_capacity - 1;
	//空过的字符数
	unsigned _gap;  //空的字符数至少1个
	//对齐后的容量
	unsigned _cap_aligned; //跟元素对齐后的容量
	//可用总容量
	unsigned _max_size;   //最大可用容量= _cap_aligned - _gap

	//内部分配的空间
	std::vector<T> _buffer;
	//缓冲区的引用
	T* _ptr{ nullptr };


    alignas(64) std::atomic<TYPE1> write_pos{ 0 }; 

	alignas(64) std::atomic<TYPE1> read_pos{ 0 };

	TYPE1 write_lock_len{ 0 };

	TYPE1 read_lock_len{ 0 };

};

using ByteRing = RingBuffer2<>;