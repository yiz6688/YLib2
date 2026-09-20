#pragma once
#include"base_config.hpp"
#include<atomic>
#include<vector>
#include<algorithm>
#include<cstring>
#include<limits>
#include<stdexcept>
#include<type_traits>

/**
*环形缓冲区类(按帧管理)
* 1、支持单生产者单消费者 无锁读写
* 2、任一时刻，可读帧数+可写帧数 = 最大可用帧数(_maxFrames)
* 3、帧(frame): 大小为 T 的整数倍(字节), 是读写的基本单位, 所有读写方法都要求帧对齐
* 4、帧数量(_frameCount) 必须为 2 的幂次: 位置按帧计数, 直接取模即可利用无符号回绕
* 5、最大可用帧数(_maxFrames, 门限) 可精确控制, 默认 _frameCount(空间可用满);
*    位置单调计数, 空/满由可读帧数为 0 / 达到门限区分
*/
template<typename T=char>
class RingBuffer2
{

private:
	using TYPE1 = unsigned;	

	static_assert(std::is_trivially_copyable_v<T>, "RingBuffer2 仅支持平凡可拷贝类型(T), 内部按字节拷贝");

public:
	//内部申请空间: frameCount 为帧数量(2 的幂次), frameSize 为帧大小(字节, sizeof(T) 的整数倍)
	RingBuffer2(unsigned frameCount, unsigned frameSize = sizeof(T))
        : _frameCount{ frameCount }, _frameSize{ frameSize }
    {
        this->_initCommon();
        this->_buffer.resize(this->_capacity);
        this->_ptr = this->_buffer.data();
    }
	//使用外部 char* 空间包装(按字节缓冲区传入, 约束一致); T=char 时与 T* 构造重合, 只提供 T* 版本
	//read/write(字节拷贝)无对齐要求; 但 getReadBuffer/getWriteBuffer 返回 span<T>, 直接按 T 类型访问时
	//外部缓冲区仍需按 alignof(T) 对齐
	template<typename U = T, std::enable_if_t<!std::is_same_v<U, char>, int> = 0>
	RingBuffer2(char* buffer, unsigned frameCount, unsigned frameSize = sizeof(T))
        : _frameCount{ frameCount }, _frameSize{ frameSize }, _buffer(), _ptr{ reinterpret_cast<T*>(buffer) }
    {
        if(buffer == nullptr)
        {
            throw std::runtime_error("外部缓冲区不能为空");
        }
        this->_initCommon();
    }
	//使用外部 T* 空间包装(帧大小约束一致)
	RingBuffer2(T* buffer, unsigned frameCount, unsigned frameSize = sizeof(T))
        : _frameCount{ frameCount }, _frameSize{ frameSize }, _buffer(), _ptr{ buffer }
    {
        if(buffer == nullptr)
        {
            throw std::runtime_error("外部缓冲区不能为空");
        }
        this->_initCommon();
    }

	~RingBuffer2()
    {
        this->_ptr = nullptr;
    }

	RingBuffer2(RingBuffer2&& value) noexcept
    {
        this->_frameCount = value._frameCount;
        this->_frameSize = value._frameSize;
        this->_elemsPerFrame = value._elemsPerFrame;
        this->_capacity = value._capacity;
        this->_mask = value._mask;
        this->_maxFrames = value._maxFrames;
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
        this->_frameCount = value._frameCount;
        this->_frameSize = value._frameSize;
        this->_elemsPerFrame = value._elemsPerFrame;
        this->_capacity = value._capacity;
        this->_mask = value._mask;
        this->_maxFrames = value._maxFrames;
        this->_buffer = std::move(value._buffer);
        this->_ptr = value._ptr;
        this->write_pos.store(value.write_pos.load());
        this->read_pos.store(value.read_pos.load());
        this->write_lock_len = value.write_lock_len;
        this->read_lock_len = value.read_lock_len;
        return *this;
    }

	//设置最大可用帧数(门限): 必须大于 0 且不超过 _frameCount(位置单调计数, 空间可用满)
	void setLimit(TYPE1 limit)
    {
        if(limit <= 0)
        {
            throw std::runtime_error("limit 必须大于 0");
        }
        if(limit > this->_frameCount)
        {
            throw std::runtime_error("limit 不能超过帧数量");
        }
        auto wpos = this->write_pos.load(std::memory_order_relaxed);
        auto rpos = this->read_pos.load(std::memory_order_acquire);
        if(limit < this->calcReadableFrames(wpos, rpos))
        {
            throw std::runtime_error("limit 不能小于当前未读帧数");
        }
        this->_maxFrames = limit;
    }


	//从环形缓冲区读取数据, 帧对齐(不足一帧的部分被截断), 返回读出的 T 元素数
	int read(T* buffer, int size)
    {
        return this->read(buffer, 0, size);
    }

	int read(T* buffer, int offset, int size)
    {
        if(this->read_lock_len != 0 || size <= 0)
        {
            return 0;
        }

        //帧对齐: 向下取整到整帧
        size = (size / this->_elemsPerFrame) * this->_elemsPerFrame;
        if(size == 0)
        {
            return 0;
        }

        auto wpos = this->write_pos.load(std::memory_order_acquire);
        auto rpos = this->read_pos.load(std::memory_order_relaxed);

        auto readableFrames = this->calcReadableFrames(wpos, rpos); //可读帧数
        if(readableFrames == 0)
        {
            return 0;
        }
        TYPE1 wantFrames = static_cast<TYPE1>(size) / this->_elemsPerFrame;
        if(readableFrames > wantFrames)
        {
            readableFrames = wantFrames;
        }

        auto readPos = rpos & this->_mask; //帧索引
        auto readElem = readPos * this->_elemsPerFrame; //元素偏移

        auto tailFrames = this->_frameCount - readPos; //读指针到尾部的帧数
        auto size1 = std::min(tailFrames, readableFrames); //第一段: 尾部整段
        auto size2 = readableFrames - size1;              //第二段: 回绕到头部

        std::memcpy(buffer + offset, this->_ptr + readElem, size1 * this->_elemsPerFrame * sizeof(T));

        if(size2 > 0)
        {
            offset += size1 * this->_elemsPerFrame;
            std::memcpy(buffer + offset, this->_ptr, size2 * this->_elemsPerFrame * sizeof(T));
        }

        rpos += readableFrames;
        this->read_pos.store(rpos, std::memory_order_release);

        return static_cast<int>(readableFrames * this->_elemsPerFrame);
    }


	//写入环形缓冲区, 帧对齐(不足一帧的部分被截断), 返回写入的 T 元素数
	int write(const T* data, int size)
    {
        return this->write(data, 0, size);
    }
	
	int write(const T* data, int offset, int size)
    {
        if(this->write_lock_len != 0 || size <= 0)
        {
            return 0;
        }

        //帧对齐: 向下取整到整帧
        size = (size / this->_elemsPerFrame) * this->_elemsPerFrame;
        if(size == 0)
        {
            return 0;
        }

        auto wpos = this->write_pos.load(std::memory_order_relaxed);
        auto rpos = this->read_pos.load(std::memory_order_acquire);

        auto writeableFrames = this->calcWriteableFrames(wpos, rpos); //可写帧数
        if(writeableFrames == 0)
        {
            return 0;
        }
        TYPE1 wantFrames = static_cast<TYPE1>(size) / this->_elemsPerFrame;
        if(writeableFrames > wantFrames)
        {
            writeableFrames = wantFrames;
        }

        auto writePos = wpos & this->_mask; //帧索引
        auto writeElem = writePos * this->_elemsPerFrame; //元素偏移

        auto tailFrames = this->_frameCount - writePos; //写指针到尾部的帧数
        auto size1 = std::min(tailFrames, writeableFrames); //第一段: 尾部整段
        auto size2 = writeableFrames - size1;              //第二段: 回绕到头部

        std::memcpy(this->_ptr + writeElem, data + offset, size1 * this->_elemsPerFrame * sizeof(T));

        if(size2 > 0)
        {
            offset += size1 * this->_elemsPerFrame;
            std::memcpy(this->_ptr, data + offset, size2 * this->_elemsPerFrame * sizeof(T)); //从头部开始写
        }

        wpos += writeableFrames; //修正写指针(帧)
        this->write_pos.store(wpos, std::memory_order_release);

        return static_cast<int>(writeableFrames * this->_elemsPerFrame);
    }

	//从另一个环形缓冲区读
	int readFrom(RingBuffer2& ring, int size)
    {
        if (this == &ring || size <= 0)
        {
            return 0;
        }

        int nRead = static_cast<int>(ring.getReadableBytes());
        if (nRead > size)
        {
            nRead = size;
        }
        if (nRead <= 0)
        {
            return 0;
        }
        //两侧帧的 T 元素数可能不同, 对齐到两者的最小公倍数, 否则任一方坐标会漂移
        unsigned ea = this->_elemsPerFrame;
        unsigned eb = ring._elemsPerFrame;
        unsigned a = ea, b = eb;
        while (b != 0) { unsigned t = a % b; a = b; b = t; }
        unsigned lcm = ea / a * eb;
        nRead = (nRead / static_cast<int>(lcm)) * static_cast<int>(lcm);
        if (nRead <= 0)
        {
            return 0;
        }

        std::vector<T> tmp(static_cast<size_t>(nRead));
        int got = ring.read(tmp.data(), nRead);
        if (got <= 0)
        {
            return 0;
        }
        return this->write(tmp.data(), got);
    }

	//写入到另一个环形缓冲区
	int writeTo(RingBuffer2& ring, int size)
    {
        return ring.readFrom(*this, size);
    }


	span_ns::span<T> getWriteBuffer(TYPE1 size)
    {
        auto wpos = this->write_pos.load(std::memory_order_relaxed);
        auto writePos = wpos & this->_mask; //帧索引
        auto wptr = this->_ptr + writePos * this->_elemsPerFrame; //写指针起始位置

        //帧对齐: 向下取整到整帧
        size = (size / this->_elemsPerFrame) * this->_elemsPerFrame;

        if(size == 0 || this->write_lock_len != 0)
        {
            return span_ns::span<T>(wptr, static_cast<std::size_t>(0));
        }

        auto rpos = this->read_pos.load(std::memory_order_acquire);

        auto writeableFrames = this->calcWriteableFrames(wpos, rpos); //计算剩余可写帧数
        TYPE1 wantFrames = size / this->_elemsPerFrame;
        if(writeableFrames > wantFrames)
        {
            writeableFrames = wantFrames;
        }

        auto tailFrames = this->_frameCount - writePos; //写指针到尾部的帧数
        this->write_lock_len = std::min(tailFrames, writeableFrames); //锁定不超过尾部, 避免回绕

        //write_lock_len 为锁定帧数, span<T> 按 T 元素计数
        return span_ns::span<T>(wptr, this->write_lock_len * this->_elemsPerFrame);
    }

	int releaseWriteBuffer(TYPE1 size)
    {
        if(size == 0 || this->write_lock_len == 0)
        {
            return 0;
        }
        else if(size == std::numeric_limits<TYPE1>::max())
        {
            size = this->write_lock_len * this->_elemsPerFrame; //释放全部
        }
        //帧对齐: 向下取整到整帧
        size = (size / this->_elemsPerFrame) * this->_elemsPerFrame;

        if(size > this->write_lock_len * this->_elemsPerFrame)
        {
            size = this->write_lock_len * this->_elemsPerFrame;
        }

        auto releaseFrames = size / this->_elemsPerFrame;

        auto wpos = this->write_pos.load(std::memory_order_relaxed);
        wpos += releaseFrames;

        this->write_lock_len -= releaseFrames;
        this->write_pos.store(wpos, std::memory_order_release);
        //releaseSize 为 T 元素数, 直接返回
        return static_cast<int>(releaseFrames * this->_elemsPerFrame);
    }

	int releaseWriteBuffer()
    {
        return this->releaseWriteBuffer(-1);
    }

	span_ns::span<T> getReadBuffer(TYPE1 size)
    {
        auto rpos = this->read_pos.load(std::memory_order_relaxed);

        auto readPos = rpos & this->_mask; //帧索引
        auto rptr = this->_ptr + readPos * this->_elemsPerFrame; //读指针起始位置

        //帧对齐: 向下取整到整帧
        size = (size / this->_elemsPerFrame) * this->_elemsPerFrame;

        if(size == 0 || this->read_lock_len != 0)
        {
            return span_ns::span<T>(rptr, static_cast<std::size_t>(0));
        }
        
        auto wpos = this->write_pos.load(std::memory_order_acquire);

        auto readableFrames = this->calcReadableFrames(wpos, rpos); //计算可读帧数
        TYPE1 wantFrames = size / this->_elemsPerFrame;
        if(readableFrames > wantFrames)
        {
            readableFrames = wantFrames;
        }

        auto tailFrames = this->_frameCount - readPos; //读指针到尾部的帧数
        this->read_lock_len = std::min(tailFrames, readableFrames); //锁定不超过尾部, 避免回绕

        //read_lock_len 为锁定帧数, span<T> 按 T 元素计数
        return span_ns::span<T>(rptr, this->read_lock_len * this->_elemsPerFrame);
    }

	int releaseReadBuffer(TYPE1 size)
    {
        if(size == 0 || this->read_lock_len == 0)
        {
            return 0;
        }
        else if(size == std::numeric_limits<TYPE1>::max())
        {
            size = this->read_lock_len * this->_elemsPerFrame; //释放全部
        }
        //帧对齐: 向下取整到整帧
        size = (size / this->_elemsPerFrame) * this->_elemsPerFrame;

        if(size > this->read_lock_len * this->_elemsPerFrame)
        {
            size = this->read_lock_len * this->_elemsPerFrame;
        }

        auto releaseFrames = size / this->_elemsPerFrame;

        auto rpos = this->read_pos.load(std::memory_order_relaxed);
        rpos += releaseFrames;

        this->read_lock_len -= releaseFrames;
        this->read_pos.store(rpos, std::memory_order_release);
        //releaseSize 为 T 元素数, 直接返回
        return static_cast<int>(releaseFrames * this->_elemsPerFrame);
    }

	int releaseReadBuffer()
    {
        return this->releaseReadBuffer(-1);
    }



private:
	//公共初始化: 校验并计算派生量(frameCount 2 的幂次, frameSize 为 sizeof(T) 整数倍)
	void _initCommon()
    {
        if(this->_frameCount == 0 || (this->_frameCount & (this->_frameCount - 1)) != 0)
        {
            throw std::runtime_error("帧数量必须是 2 的幂次");
        }
        if(this->_frameSize == 0)
        {
            throw std::runtime_error("帧大小不能为 0");
        }
        if(this->_frameSize % sizeof(T) != 0)
        {
            throw std::runtime_error("帧大小必须是 sizeof(T) 的整数倍");
        }
        this->_elemsPerFrame = this->_frameSize / sizeof(T);
        if(this->_frameCount > std::numeric_limits<unsigned>::max() / this->_elemsPerFrame)
        {
            throw std::runtime_error("帧数量 × 每帧元素数 超出 unsigned 范围");
        }
        this->_capacity = this->_frameCount * this->_elemsPerFrame;
        this->_mask = this->_frameCount - 1;
        this->_maxFrames = this->_frameCount; //全部可用, 门限上限即帧数量
    }

	TYPE1 calcReadableFrames(TYPE1 wpos, TYPE1 rpos)
	{
		//位置按帧单调递增, 差值即真实可读帧数(无符号回绕下仍精确, 只要差值 < 2^32);
		//mask 仅用于帧索引计算, 这里不能取模, 否则满时(差值=帧数量)会误判为空
		return wpos - rpos;
	}

	TYPE1 calcWriteableFrames(TYPE1 wpos, TYPE1 rpos)
	{
		auto readableFrames = this->calcReadableFrames(wpos, rpos);
		return this->_maxFrames - readableFrames;
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
		return static_cast<size_t>(this->calcReadableFrames(wpos, rpos)) * this->_elemsPerFrame;
	}

	size_t getWriteableBytes()
	{
		auto wpos = this->write_pos.load(std::memory_order_acquire);
		auto rpos = this->read_pos.load(std::memory_order_acquire);
		return static_cast<size_t>(this->calcWriteableFrames(wpos, rpos)) * this->_elemsPerFrame;
	}

	size_t getReadableFrames()
	{
		auto wpos = this->write_pos.load(std::memory_order_acquire);
		auto rpos = this->read_pos.load(std::memory_order_acquire);
		return this->calcReadableFrames(wpos, rpos);
	}

	size_t getWriteableFrames()
	{
		auto wpos = this->write_pos.load(std::memory_order_acquire);
		auto rpos = this->read_pos.load(std::memory_order_acquire);
		return this->calcWriteableFrames(wpos, rpos);
	}

	size_t getCapacity()
	{
		return this->_capacity; //T 元素总数
	}

	size_t getFrameCount()
	{
		return this->_frameCount;
	}

	size_t getFrameSize()
	{
		return this->_frameSize; //字节
	}

	size_t getMaxFrames()
	{
		return this->_maxFrames;
	}




private:

	//帧数量(2 的幂次)
	unsigned _frameCount;
	//帧大小(字节, sizeof(T) 的整数倍)
	unsigned _frameSize;
	//每帧的 T 元素个数 = _frameSize / sizeof(T)
	unsigned _elemsPerFrame;
	//T 元素总容量 = _frameCount * _elemsPerFrame
	unsigned _capacity;
	//帧索引取余掩码 = _frameCount - 1
	unsigned _mask;
	//最大可用帧数(门限), <= _frameCount
	unsigned _maxFrames;

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
