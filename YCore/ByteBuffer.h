#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// 字节序（默认小端，与主机一致）
enum class ByteOrder : bool { LittleEndian = true, BigEndian = false };

class BufferUnderflowException : public std::runtime_error
{
public:
    BufferUnderflowException() : std::runtime_error("buffer underflow") {}
};

class BufferOverflowException : public std::runtime_error
{
public:
    BufferOverflowException() : std::runtime_error("buffer overflow") {}
};

class ReadOnlyBufferException : public std::logic_error
{
public:
    ReadOnlyBufferException() : std::logic_error("read-only buffer") {}
};

class IndexOutOfBoundsException : public std::out_of_range
{
public:
    explicit IndexOutOfBoundsException(const std::string& msg)
        : std::out_of_range(msg)
    {
    }
};

// 简单的字节缓冲区：内置一段固定内存，从字节数组中顺序/绝对读写各类型
// （readInt16/readInt32/readFloat/readString...），支持字节序切换与链式写。
class ByteBuffer final
{
public:
    // ---------- 构造 ----------
    static ByteBuffer allocate(std::size_t capacity); // 自持有内存
    static ByteBuffer wrap(void* data, std::size_t length);       // 外部可写
    static ByteBuffer wrap(const void* data, std::size_t length); // 外部只读

    ByteBuffer() = default;
    ~ByteBuffer() = default;
    ByteBuffer(const ByteBuffer&) = default;
    ByteBuffer& operator=(const ByteBuffer&) = default;
    ByteBuffer(ByteBuffer&&) = default;
    ByteBuffer& operator=(ByteBuffer&&) = default;

    // ---------- 容量 / 游标 ----------
    std::size_t capacity() const noexcept { return m_capacity; }
    std::size_t position() const noexcept { return m_position; }
    ByteBuffer& position(std::size_t p);
    std::size_t limit() const noexcept { return m_limit; }
    ByteBuffer& limit(std::size_t l);
    std::size_t remaining() const noexcept { return m_limit - m_position; }
    bool hasRemaining() const noexcept { return m_position < m_limit; }
    bool isReadOnly() const noexcept { return m_readOnly; }
    ByteBuffer& flip();  // 写完转读：limit=position, position=0
    ByteBuffer& rewind(); // 重新读：position=0
    ByteBuffer& clear();  // 重置：position=0, limit=capacity

    // ---------- 字节序 ----------
    ByteOrder order() const noexcept { return m_order; }
    ByteBuffer& order(ByteOrder o) noexcept
    {
        m_order = o;
        return *this;
    }
    ByteBuffer& littleEndian() noexcept
    {
        return order(ByteOrder::LittleEndian);
    }
    ByteBuffer& bigEndian() noexcept { return order(ByteOrder::BigEndian); }

    // ---------- 底层内存 ----------
    const char* data() const noexcept { return m_data; }
    char* data();

    // 预留 len 字节供外部写入：返回 [position, position+len)，等价于写入，position 自动推进；
    // 实际数据拷贝由外部保证。
    std::span<char> writableSpan(std::size_t len);
    // 读取 len 字节视图：返回 [position, position+len)，等价于读取，position 自动推进。
    std::span<const char> readableSpan(std::size_t len);

    // ---------- 读（相对，推进 position） ----------
    char readByte();
    void readBytes(void* dst, std::size_t len);
    bool readBool();
    std::int16_t readInt16();
    std::uint16_t readUInt16();
    std::int32_t readInt32();
    std::uint32_t readUInt32();
    std::int64_t readInt64();
    std::uint64_t readUInt64();
    float readFloat();
    double readDouble();
    std::string readString();                // uint32 长度前缀 + 内容
    std::string readString(std::size_t len); // 直接读 len 字节（无前缀）

    // ---------- 写（相对，链式返回 *this） ----------
    ByteBuffer& writeByte(char b);
    ByteBuffer& writeBytes(const void* src, std::size_t len);
    ByteBuffer& writeBool(bool b);
    ByteBuffer& writeInt16(std::int16_t v);
    ByteBuffer& writeUInt16(std::uint16_t v);
    ByteBuffer& writeInt32(std::int32_t v);
    ByteBuffer& writeUInt32(std::uint32_t v);
    ByteBuffer& writeInt64(std::int64_t v);
    ByteBuffer& writeUInt64(std::uint64_t v);
    ByteBuffer& writeFloat(float v);
    ByteBuffer& writeDouble(double v);
    ByteBuffer& writeString(const std::string& s); // uint32 长度前缀 + 内容

    static void UnitTest();

private:
    // 按字节序逐字节搬运，天然完成大小端翻转（经 char* 拷贝位模式，与符号无关）
    template <typename T>
    static T load(const char* p, ByteOrder bo) noexcept
    {
        T v;
        char* dst = reinterpret_cast<char*>(&v);
        if (bo == ByteOrder::LittleEndian)
        {
            for (std::size_t i = 0; i < sizeof(T); ++i)
                dst[i] = p[i];
        }
        else
        {
            for (std::size_t i = 0; i < sizeof(T); ++i)
                dst[i] = p[sizeof(T) - 1 - i];
        }
        return v;
    }

    template <typename T>
    static void store(char* p, T v, ByteOrder bo) noexcept
    {
        const char* src = reinterpret_cast<const char*>(&v);
        if (bo == ByteOrder::LittleEndian)
        {
            for (std::size_t i = 0; i < sizeof(T); ++i)
                p[i] = src[i];
        }
        else
        {
            for (std::size_t i = 0; i < sizeof(T); ++i)
                p[sizeof(T) - 1 - i] = src[i];
        }
    }

    void ensureReadable(std::size_t n) const;
    void ensureWritable(std::size_t n) const;

private:
    std::vector<char> m_storage; // 自持有内存（allocate 时使用）
    char* m_data = nullptr;
    std::size_t m_capacity = 0;
    std::size_t m_position = 0;
    std::size_t m_limit = 0;
    bool m_readOnly = false;
    ByteOrder m_order = ByteOrder::LittleEndian;
};
