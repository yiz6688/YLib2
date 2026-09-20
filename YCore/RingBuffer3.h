#pragma once
#include"base_config.hpp"
#include<atomic>
#include<vector>
#include<algorithm>
#include<cstring>
#include<limits>
#include<stdexcept>
#include<type_traits>
#include<cstdint>

/**
*环形缓冲区类(可覆盖 / overwrite)
* 1、支持单生产者单消费者 无锁读写, 写侧永不阻塞
* 2、写快读慢时, 新数据覆盖最旧数据; 读侧检测到落后超过一个容量时快进, 丢弃已被覆盖的帧
* 3、帧(frame): 大小为 T 的整数倍(字节), 是读写的基本单位, 所有读写方法都要求帧对齐
* 4、帧数量(_frameCount) 必须为 2 的幂次, 帧索引用 mask 取模
* 5、位置为 uint64 无限递增的原子计数(write_pos/read_pos), 覆盖效果通过读侧
*    "写-读 > 帧数量则快进" 实现, 无符号回绕下差值恒精确
*/
template<typename T=char>
class RingBuffer3
{

private:
	using TYPE1 = uint64_t;

	static_assert(std::is_trivially_copyable_v<T>, "RingBuffer3 仅支持平凡可拷贝类型(T), 内部按字节拷贝");

public:
	//内部申请空间: frameCount 为帧数量(2 的幂次), frameSize 为帧大小(字节, sizeof(T) 的整数倍)
	RingBuffer3(unsigned frameCount, unsigned frameSize = sizeof(T))
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
	RingBuffer3(char* buffer, unsigned frameCount, unsigned frameSize = sizeof(T))
        : _frameCount{ frameCount }, _frameSize{ frameSize }, _buffer(), _ptr{ reinterpret_cast<T*>(buffer) }
    {
        if(buffer == nullptr)
        {
            throw std::runtime_error("外部缓冲区不能为空");
        }
        this->_initCommon();
    }
	//使用外部 T* 空间包装(帧大小约束一致)
	RingBuffer3(T* buffer, unsigned frameCount, unsigned frameSize = sizeof(T))
        : _frameCount{ frameCount }, _frameSize{ frameSize }, _buffer(), _ptr{ buffer }
    {
        if(buffer == nullptr)
        {
            throw std::runtime_error("外部缓冲区不能为空");
        }
        this->_initCommon();
    }

	~RingBuffer3()
    {
        this->_ptr = nullptr;
    }

	RingBuffer3(RingBuffer3&& value) noexcept
    {
        this->_frameCount = value._frameCount;
        this->_frameSize = value._frameSize;
        this->_elemsPerFrame = value._elemsPerFrame;
        this->_capacity = value._capacity;
        this->_mask = value._mask;
        this->_buffer = std::move(value._buffer);
        this->_ptr = value._ptr;
        this->write_pos.store(value.write_pos.load());
        this->read_pos.store(value.read_pos.load());
        
        this->write_lock_len = value.write_lock_len;
        this->read_lock_len = value.read_lock_len;
    }

	RingBuffer3& operator=(RingBuffer3&& value) noexcept
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
        this->_buffer = std::move(value._buffer);
        this->_ptr = value._ptr;
        this->write_pos.store(value.write_pos.load());
        this->read_pos.store(value.read_pos.load());
        this->write_lock_len = value.write_lock_len;
        this->read_lock_len = value.read_lock_len;
        return *this;
    }


	//从环形缓冲区读取数据(覆盖模式: 若写侧已覆盖最旧数据则快进跳过), 帧对齐, 返回读出的 T 元素数
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

        //覆盖处理: 写-读超过帧数量时, 最旧帧已被覆盖, 快进到最新帧数量起点
        auto avail = wpos - rpos;
        if(avail > this->_frameCount)
        {
            rpos = wpos - this->_frameCount;
            this->read_pos.store(rpos, std::memory_order_relaxed);
            avail = this->_frameCount;
        }
        if(avail == 0)
        {
            return 0;
        }
        TYPE1 wantFrames = static_cast<TYPE1>(size) / this->_elemsPerFrame;
        if(avail > wantFrames)
        {
            avail = wantFrames;
        }

        auto readPos = rpos & this->_mask; //帧索引
        auto readElem = readPos * this->_elemsPerFrame; //元素偏移

        auto tailFrames = this->_frameCount - readPos; //读指针到尾部的帧数
        auto size1 = std::min(tailFrames, avail);      //第一段: 尾部整段
        auto size2 = avail - size1;                    //第二段: 回绕到头部

        std::memcpy(buffer + offset, this->_ptr + readElem, size1 * this->_elemsPerFrame * sizeof(T));

        if(size2 > 0)
        {
            offset += size1 * this->_elemsPerFrame;
            std::memcpy(buffer + offset, this->_ptr, size2 * this->_elemsPerFrame * sizeof(T));
        }

        rpos += avail;
        this->read_pos.store(rpos, std::memory_order_relaxed);

        return static_cast<int>(avail * this->_elemsPerFrame);
    }


	//写入环形缓冲区(覆盖模式: 永不阻塞, 空间不足时覆盖最旧数据), 帧对齐, 返回写入的 T 元素数
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

        //覆盖模式: 一次最多写满整个缓冲区, 写侧从不因读侧而阻塞
        TYPE1 wantFrames = static_cast<TYPE1>(size) / this->_elemsPerFrame;
        if(wantFrames > this->_frameCount)
        {
            wantFrames = this->_frameCount;
        }

        auto wpos = this->write_pos.load(std::memory_order_relaxed);

        auto writePos = wpos & this->_mask; //帧索引
        auto writeElem = writePos * this->_elemsPerFrame; //元素偏移

        auto tailFrames = this->_frameCount - writePos; //写指针到尾部的帧数
        auto size1 = std::min(tailFrames, wantFrames);  //第一段: 尾部整段
        auto size2 = wantFrames - size1;                //第二段: 回绕到头部

        std::memcpy(this->_ptr + writeElem, data + offset, size1 * this->_elemsPerFrame * sizeof(T));

        if(size2 > 0)
        {
            offset += size1 * this->_elemsPerFrame;
            std::memcpy(this->_ptr, data + offset, size2 * this->_elemsPerFrame * sizeof(T)); //从头部开始写
        }

        wpos += wantFrames; //修正写指针(帧)
        this->write_pos.store(wpos, std::memory_order_release);

        return static_cast<int>(wantFrames * this->_elemsPerFrame);
    }

	//从另一个环形缓冲区读
	int readFrom(RingBuffer3& ring, int size)
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
	int writeTo(RingBuffer3& ring, int size)
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

        //覆盖模式: 一次最多锁定整个缓冲区
        TYPE1 wantFrames = size / this->_elemsPerFrame;
        if(wantFrames > this->_frameCount)
        {
            wantFrames = this->_frameCount;
        }

        auto tailFrames = this->_frameCount - writePos; //写指针到尾部的帧数
        this->write_lock_len = std::min(tailFrames, wantFrames); //锁定不超过尾部, 避免回绕

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
        //帧对齐: 向下取整到整帧
        size = (size / this->_elemsPerFrame) * this->_elemsPerFrame;

        if(size == 0 || this->read_lock_len != 0)
        {
            return span_ns::span<T>(this->_ptr, static_cast<std::size_t>(0));
        }

        auto wpos = this->write_pos.load(std::memory_order_acquire);
        auto rpos = this->read_pos.load(std::memory_order_relaxed);

        //覆盖处理: 写-读超过帧数量时, 快进跳过已被覆盖的最旧帧
        auto avail = wpos - rpos;
        if(avail > this->_frameCount)
        {
            rpos = wpos - this->_frameCount;
            this->read_pos.store(rpos, std::memory_order_relaxed);
            avail = this->_frameCount;
        }
        if(avail == 0)
        {
            return span_ns::span<T>(this->_ptr, static_cast<std::size_t>(0));
        }
        TYPE1 wantFrames = size / this->_elemsPerFrame;
        if(avail > wantFrames)
        {
            avail = wantFrames;
        }

        auto readPos = rpos & this->_mask; //帧索引
        auto rptr = this->_ptr + readPos * this->_elemsPerFrame; //读指针起始位置

        auto tailFrames = this->_frameCount - readPos; //读指针到尾部的帧数
        this->read_lock_len = std::min(tailFrames, avail); //锁定不超过尾部, 避免回绕

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
        this->read_pos.store(rpos, std::memory_order_relaxed);
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
    }


public:

	//清空(覆盖模式: 直接丢弃全部数据)
	void reset()
	{
		this->read_pos.store(0, std::memory_order_relaxed);
		this->write_pos.store(0, std::memory_order_relaxed);
		this->write_lock_len = 0;
		this->read_lock_len = 0;
	}



	size_t getReadableBytes()
	{
		return this->getReadableFrames() * this->_elemsPerFrame;
	}

	size_t getWriteableBytes()
	{
		//覆盖模式: 写侧恒可写满整个缓冲区(覆盖旧数据)
		return static_cast<size_t>(this->_frameCount) * this->_elemsPerFrame;
	}

	size_t getReadableFrames()
	{
		auto wpos = this->write_pos.load(std::memory_order_acquire);
		auto rpos = this->read_pos.load(std::memory_order_relaxed);
		auto avail = wpos - rpos;
		return avail > this->_frameCount ? this->_frameCount : static_cast<size_t>(avail);
	}

	size_t getWriteableFrames()
	{
		//覆盖模式: 写侧恒可写满整个缓冲区(覆盖旧数据)
		return this->_frameCount;
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

	//内部分配的空间
	std::vector<T> _buffer;
	//缓冲区的引用
	T* _ptr{ nullptr };


    alignas(64) std::atomic<TYPE1> write_pos{ 0 }; 

	alignas(64) std::atomic<TYPE1> read_pos{ 0 };

	TYPE1 write_lock_len{ 0 };

	TYPE1 read_lock_len{ 0 };

};

using ByteRing3 = RingBuffer3<>;
