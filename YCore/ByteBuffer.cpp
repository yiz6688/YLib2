#include"base_config.hpp"
#include "ByteBuffer.h"

#include <cstring>

// ---------- 构造 ----------

ByteBuffer ByteBuffer::allocate(std::size_t capacity)
{
    ByteBuffer b;
    b.m_storage.resize(capacity);
    b.m_data = b.m_storage.data();
    b.m_capacity = capacity;
    b.m_limit = capacity;
    return b;
}

ByteBuffer ByteBuffer::wrap(void* data, std::size_t length)
{
    ByteBuffer b;
    b.m_data = static_cast<char*>(data);
    b.m_capacity = length;
    b.m_limit = length;
    return b;
}

ByteBuffer ByteBuffer::wrap(const void* data, std::size_t length)
{
    ByteBuffer b = wrap(const_cast<void*>(data), length);
    b.m_readOnly = true;
    return b;
}

// ---------- 容量 / 游标 ----------

ByteBuffer& ByteBuffer::position(std::size_t p)
{
    if (p > m_limit)
    {
        throw IndexOutOfBoundsException("position > limit");
    }
    m_position = p;
    return *this;
}

ByteBuffer& ByteBuffer::limit(std::size_t l)
{
    if (l > m_capacity)
    {
        throw IndexOutOfBoundsException("limit > capacity");
    }
    m_limit = l;
    if (m_position > m_limit)
    {
        m_position = m_limit;
    }
    return *this;
}

ByteBuffer& ByteBuffer::flip()
{
    m_limit = m_position;
    m_position = 0;
    return *this;
}

ByteBuffer& ByteBuffer::rewind()
{
    m_position = 0;
    return *this;
}

ByteBuffer& ByteBuffer::clear()
{
    m_position = 0;
    m_limit = m_capacity;
    return *this;
}

char* ByteBuffer::data()
{
    if (m_readOnly)
    {
        throw ReadOnlyBufferException();
    }
    return m_data;
}

span_ns::span<char> ByteBuffer::writableSpan(std::size_t len)
{
    ensureWritable(len);
    char* p = m_data + m_position;
    m_position += len;
    return span_ns::span<char>(p, len);
}

span_ns::span<const char> ByteBuffer::readableSpan(std::size_t len)
{
    ensureReadable(len);
    const char* p = m_data + m_position;
    m_position += len;
    return span_ns::span<const char>(p, len);
}

// ---------- 越界检查 ----------

void ByteBuffer::ensureReadable(std::size_t n) const
{
    if (n > remaining())
    {
        throw BufferUnderflowException();
    }
}

void ByteBuffer::ensureWritable(std::size_t n) const
{
    if (m_readOnly)
    {
        throw ReadOnlyBufferException();
    }
    if (n > remaining())
    {
        throw BufferOverflowException();
    }
}

// ---------- 读（相对） ----------

char ByteBuffer::readByte()
{
    ensureReadable(1);
    return m_data[m_position++];
}

void ByteBuffer::readBytes(void* dst, std::size_t len)
{
    ensureReadable(len);
    std::memcpy(dst, m_data + m_position, len);
    m_position += len;
}

bool ByteBuffer::readBool()
{
    return readByte() != 0;
}

std::int16_t ByteBuffer::readInt16()
{
    ensureReadable(2);
    std::int16_t v = load<std::int16_t>(m_data + m_position, m_order);
    m_position += 2;
    return v;
}

std::uint16_t ByteBuffer::readUInt16()
{
    ensureReadable(2);
    std::uint16_t v = load<std::uint16_t>(m_data + m_position, m_order);
    m_position += 2;
    return v;
}

std::int32_t ByteBuffer::readInt32()
{
    ensureReadable(4);
    std::int32_t v = load<std::int32_t>(m_data + m_position, m_order);
    m_position += 4;
    return v;
}

std::uint32_t ByteBuffer::readUInt32()
{
    ensureReadable(4);
    std::uint32_t v = load<std::uint32_t>(m_data + m_position, m_order);
    m_position += 4;
    return v;
}

std::int64_t ByteBuffer::readInt64()
{
    ensureReadable(8);
    std::int64_t v = load<std::int64_t>(m_data + m_position, m_order);
    m_position += 8;
    return v;
}

std::uint64_t ByteBuffer::readUInt64()
{
    ensureReadable(8);
    std::uint64_t v = load<std::uint64_t>(m_data + m_position, m_order);
    m_position += 8;
    return v;
}

float ByteBuffer::readFloat()
{
    ensureReadable(4);
    float v = load<float>(m_data + m_position, m_order);
    m_position += 4;
    return v;
}

double ByteBuffer::readDouble()
{
    ensureReadable(8);
    double v = load<double>(m_data + m_position, m_order);
    m_position += 8;
    return v;
}

std::string ByteBuffer::readString()
{
    std::uint32_t n = readUInt32();
    ensureReadable(n);
    std::string s(m_data + m_position, n);
    m_position += n;
    return s;
}

std::string ByteBuffer::readString(std::size_t len)
{
    ensureReadable(len);
    std::string s(m_data + m_position, len);
    m_position += len;
    return s;
}

// ---------- 写（相对，链式） ----------

ByteBuffer& ByteBuffer::writeByte(char b)
{
    ensureWritable(1);
    m_data[m_position++] = b;
    return *this;
}

ByteBuffer& ByteBuffer::writeBytes(const void* src, std::size_t len)
{
    ensureWritable(len);
    std::memcpy(m_data + m_position, src, len);
    m_position += len;
    return *this;
}

ByteBuffer& ByteBuffer::writeBool(bool b)
{
    return writeByte(b ? 1 : 0);
}

ByteBuffer& ByteBuffer::writeInt16(std::int16_t v)
{
    ensureWritable(2);
    store<std::int16_t>(m_data + m_position, v, m_order);
    m_position += 2;
    return *this;
}

ByteBuffer& ByteBuffer::writeUInt16(std::uint16_t v)
{
    ensureWritable(2);
    store<std::uint16_t>(m_data + m_position, v, m_order);
    m_position += 2;
    return *this;
}

ByteBuffer& ByteBuffer::writeInt32(std::int32_t v)
{
    ensureWritable(4);
    store<std::int32_t>(m_data + m_position, v, m_order);
    m_position += 4;
    return *this;
}

ByteBuffer& ByteBuffer::writeUInt32(std::uint32_t v)
{
    ensureWritable(4);
    store<std::uint32_t>(m_data + m_position, v, m_order);
    m_position += 4;
    return *this;
}

ByteBuffer& ByteBuffer::writeInt64(std::int64_t v)
{
    ensureWritable(8);
    store<std::int64_t>(m_data + m_position, v, m_order);
    m_position += 8;
    return *this;
}

ByteBuffer& ByteBuffer::writeUInt64(std::uint64_t v)
{
    ensureWritable(8);
    store<std::uint64_t>(m_data + m_position, v, m_order);
    m_position += 8;
    return *this;
}

ByteBuffer& ByteBuffer::writeFloat(float v)
{
    ensureWritable(4);
    store<float>(m_data + m_position, v, m_order);
    m_position += 4;
    return *this;
}

ByteBuffer& ByteBuffer::writeDouble(double v)
{
    ensureWritable(8);
    store<double>(m_data + m_position, v, m_order);
    m_position += 8;
    return *this;
}

ByteBuffer& ByteBuffer::writeString(const std::string& s)
{
    writeUInt32(static_cast<std::uint32_t>(s.size()));
    writeBytes(s.data(), s.size());
    return *this;
}

