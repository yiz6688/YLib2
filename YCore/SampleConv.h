#pragma once
#include"base_config.hpp"
#include<cmath>
#include<limits>
#include<cstdint>
#include<cstring>
#include"myType.h"

//采样格式转换工具:
// 1、类型化转换(TYPED)  : 源/目标为原生类型指针(short/int/int24/float/double)
// 2、字节流转换(BYTE)   : 源/目标为 char* 小端原始字节(2/3/4字节), 逐字节拼装保证安全
// 3、int24 支持两种形态: int24 结构体(bit准确) 与 int32 容器(24位范围)
// 量化约定: int -> float 用 value/q_max, float -> int 用 round(v*q_max) 并钳制, 尺度参数应用一次
class SampleConv
{
public:
    //=====================================================================
    // 类型化转换(TYPED)
    //=====================================================================
    // int(T=short/int/int24) -> float/double
    template<typename T>
    static void IntToFloat(const T* src, int n, float* dest, float scale = 1.0f);
    template<typename T>
    static void IntToDouble(const T* src, int n, double* dest, double scale = 1.0);
    // float/double -> int(T=short/int/int24)
    template<typename T>
    static void FloatToInt(const float* src, int n, T* dest, float scale = 1.0f);
    template<typename T>
    static void DoubleToInt(const double* src, int n, T* dest, double scale = 1.0);
    // int -> int 位深转换(精确移位, 按 value bits 差)
    template<typename SRC, typename DST>
    static void IntToInt(const SRC* src, int n, DST* dest);

    // int24 以 int32 容器存放(24位范围 ±8388607), 与 IntToFloat<int>(32位范围) 区分
    static void Int24ToFloat(const int* src, int n, float* dest, float scale = 1.0f);
    static void Int24ToDouble(const int* src, int n, double* dest, double scale = 1.0);
    static void FloatToInt24(const float* src, int n, int* dest, float scale = 1.0f);
    static void DoubleToInt24(const double* src, int n, int* dest, double scale = 1.0);

    //=====================================================================
    // 字节流转换(BYTE, 小端, 逐字节拼装安全)
    //=====================================================================
    // 字节int(2/3/4字节小端) -> float/double
    static void IntBytesToFloat(const char* src, int n, float* dest, int bytesPerSample, float scale = 1.0f);
    static void IntBytesToDouble(const char* src, int n, double* dest, int bytesPerSample, double scale = 1.0);
    // float/double -> 字节int(2/3/4字节小端)
    static void FloatToIntBytes(const float* src, int n, char* dest, int bytesPerSample, float scale = 1.0f);
    static void DoubleToIntBytes(const double* src, int n, char* dest, int bytesPerSample, double scale = 1.0);
    // 字节int -> 类型int位深 / 类型int -> 字节int位深
    template<typename DST>
    static void IntBytesToInt(const char* src, int n, DST* dest, int srcBytes);
    template<typename SRC>
    static void IntToIntBytes(const SRC* src, int n, char* dest, int dstBytes);
    // 字节int(2/3/4字节小端) -> int24容器(24位范围, int32存储)
    static void IntBytesToInt24C(const char* src, int n, int* dest, int srcBytes);
    // int24容器(24位范围) -> 字节int(2/3/4字节小端)
    static void Int24CToIntBytes(const int* src, int n, char* dest, int dstBytes);

    //=====================================================================
    // 便捷接口(旧名, 委托到上述实现, 保持既有调用不变)
    //=====================================================================
    static void Int16toFloat(short* src, int n, float* dest) { IntToFloat(src, n, dest); }
    static void Int32toFloat(int* src, int n, float* dest) { IntToFloat(src, n, dest); }
    static void Int24BytetoFloat(char* src, int n, float* dest) { IntBytesToFloat(src, n, dest, 3); }
    static void FloattoInt16(float* src, int n, short* dest) { FloatToInt(src, n, dest); }
    static void FloattoInt32(float* src, int n, int* dest) { FloatToInt(src, n, dest); }
    static void FloattoInt24Byte(float* src, int n, char* dest) { FloatToIntBytes(src, n, dest, 3); }
    //按位宽分派的字节便捷接口(统一命名)
    static void Int16BytesToFloat(const char* src, int n, float* dest) { IntBytesToFloat(src, n, dest, 2); }
    static void Int24BytesToFloat(const char* src, int n, float* dest) { IntBytesToFloat(src, n, dest, 3); }
    static void Int32BytesToFloat(const char* src, int n, float* dest) { IntBytesToFloat(src, n, dest, 4); }
    static void Int16BytesToDouble(const char* src, int n, double* dest) { IntBytesToDouble(src, n, dest, 2); }
    static void Int24BytesToDouble(const char* src, int n, double* dest) { IntBytesToDouble(src, n, dest, 3); }
    static void Int32BytesToDouble(const char* src, int n, double* dest) { IntBytesToDouble(src, n, dest, 4); }
    static void FloatToInt16Bytes(const float* src, int n, char* dest) { FloatToIntBytes(src, n, dest, 2); }
    static void FloatToInt24Bytes(const float* src, int n, char* dest) { FloatToIntBytes(src, n, dest, 3); }
    static void FloatToInt32Bytes(const float* src, int n, char* dest) { FloatToIntBytes(src, n, dest, 4); }
    static void DoubleToInt16Bytes(const double* src, int n, char* dest) { DoubleToIntBytes(src, n, dest, 2); }
    static void DoubleToInt24Bytes(const double* src, int n, char* dest) { DoubleToIntBytes(src, n, dest, 3); }
    static void DoubleToInt32Bytes(const double* src, int n, char* dest) { DoubleToIntBytes(src, n, dest, 4); }

private:
    //各字节宽的量化上限(2/3/4 -> 32767/8388607/2147483647)
    static int intBytesMax(int bytesPerSample);
    //小端有符号读取(2/3/4字节)
    static int readIntLE(const char* p, int bytes);
    //小端写入(2/3/4字节)
    static void writeIntLE(char* p, int bytes, int value);
};


//=====================================================================
// 类型化转换实现
//=====================================================================

template<typename T>
inline void SampleConv::IntToFloat(const T* src, int n, float* dest, float scale)
{
    constexpr float q = static_cast<float>((std::numeric_limits<T>::max)());
    const float coeff = scale / q;
    for (int i = 0; i < n; i++)
    {
        dest[i] = static_cast<float>(src[i]) * coeff;
    }
}

template<typename T>
inline void SampleConv::IntToDouble(const T* src, int n, double* dest, double scale)
{
    constexpr double q = static_cast<double>((std::numeric_limits<T>::max)());
    const double coeff = scale / q;
    for (int i = 0; i < n; i++)
    {
        dest[i] = static_cast<double>(src[i]) * coeff;
    }
}

template<typename T>
inline void SampleConv::FloatToInt(const float* src, int n, T* dest, float scale)
{
    constexpr int q_max = (std::numeric_limits<T>::max)();
    constexpr int q_min = (std::numeric_limits<T>::min)();
    const float coeff = scale * static_cast<float>(q_max);
    for (int i = 0; i < n; i++)
    {
        int v = static_cast<int>(std::round(src[i] * coeff));
        if (v < q_min)
        {
            v = q_min;
        }
        else if (v > q_max)
        {
            v = q_max;
        }
        dest[i] = static_cast<T>(v);
    }
}

template<typename T>
inline void SampleConv::DoubleToInt(const double* src, int n, T* dest, double scale)
{
    constexpr int q_max = (std::numeric_limits<T>::max)();
    constexpr int q_min = (std::numeric_limits<T>::min)();
    const double coeff = scale * static_cast<double>(q_max);
    for (int i = 0; i < n; i++)
    {
        int v = static_cast<int>(std::round(src[i] * coeff));
        if (v < q_min)
        {
            v = q_min;
        }
        else if (v > q_max)
        {
            v = q_max;
        }
        dest[i] = static_cast<T>(v);
    }
}

//int -> int 位深转换: 按 value bits 差精确移位(拓宽<< / 缩窄>>)
template<typename SRC, typename DST>
inline void SampleConv::IntToInt(const SRC* src, int n, DST* dest)
{
    constexpr int shift = std::numeric_limits<DST>::digits - std::numeric_limits<SRC>::digits;
    for (int i = 0; i < n; i++)
    {
        long long v = static_cast<long long>(src[i]);
        if constexpr (shift > 0)
        {
            dest[i] = static_cast<DST>(v << shift);
        }
        else if constexpr (shift < 0)
        {
            dest[i] = static_cast<DST>(v >> (-shift));
        }
        else
        {
            dest[i] = static_cast<DST>(v);
        }
    }
}

inline void SampleConv::Int24ToFloat(const int* src, int n, float* dest, float scale)
{
    constexpr float q = 8388607.0f;
    const float coeff = scale / q;
    for (int i = 0; i < n; i++)
    {
        dest[i] = static_cast<float>(src[i]) * coeff;
    }
}

inline void SampleConv::Int24ToDouble(const int* src, int n, double* dest, double scale)
{
    constexpr double q = 8388607.0;
    const double coeff = scale / q;
    for (int i = 0; i < n; i++)
    {
        dest[i] = static_cast<double>(src[i]) * coeff;
    }
}

inline void SampleConv::FloatToInt24(const float* src, int n, int* dest, float scale)
{
    constexpr int q_max = 8388607;
    constexpr int q_min = -8388608;
    const float coeff = scale * 8388607.0f;
    for (int i = 0; i < n; i++)
    {
        int v = static_cast<int>(std::round(src[i] * coeff));
        if (v < q_min)
        {
            v = q_min;
        }
        else if (v > q_max)
        {
            v = q_max;
        }
        dest[i] = v;
    }
}

inline void SampleConv::DoubleToInt24(const double* src, int n, int* dest, double scale)
{
    constexpr int q_max = 8388607;
    constexpr int q_min = -8388608;
    const double coeff = scale * 8388607.0;
    for (int i = 0; i < n; i++)
    {
        int v = static_cast<int>(std::round(src[i] * coeff));
        if (v < q_min)
        {
            v = q_min;
        }
        else if (v > q_max)
        {
            v = q_max;
        }
        dest[i] = v;
    }
}


//=====================================================================
// 字节流转换实现(小端, 逐字节拼装保证安全)
//=====================================================================

inline int SampleConv::intBytesMax(int bytesPerSample)
{
    switch (bytesPerSample)
    {
    case 2: return 32767;
    case 3: return 8388607;
    case 4: return 2147483647;
    default: return 0;
    }
}

inline int SampleConv::readIntLE(const char* p, int bytes)
{
    unsigned v = 0;
    for (int k = 0; k < bytes; k++)
    {
        v |= static_cast<unsigned>(static_cast<unsigned char>(p[k])) << (8 * k);
    }
    if (bytes == 2)
    {
        return static_cast<short>(v);
    }
    if (bytes == 3)
    {
        if (v & 0x800000u)
        {
            v |= 0xFF000000u;   //符号扩展
        }
        return static_cast<int>(v);
    }
    return static_cast<int>(v);
}

inline void SampleConv::writeIntLE(char* p, int bytes, int value)
{
    for (int k = 0; k < bytes; k++)
    {
        p[k] = static_cast<char>(value >> (8 * k));
    }
}

inline void SampleConv::IntBytesToFloat(const char* src, int n, float* dest, int bytesPerSample, float scale)
{
    const int q_max = intBytesMax(bytesPerSample);
    const float coeff = scale / static_cast<float>(q_max);
    for (int i = 0; i < n; i++)
    {
        dest[i] = static_cast<float>(readIntLE(src + static_cast<size_t>(i) * bytesPerSample, bytesPerSample)) * coeff;
    }
}

inline void SampleConv::IntBytesToDouble(const char* src, int n, double* dest, int bytesPerSample, double scale)
{
    const int q_max = intBytesMax(bytesPerSample);
    const double coeff = scale / static_cast<double>(q_max);
    for (int i = 0; i < n; i++)
    {
        dest[i] = static_cast<double>(readIntLE(src + static_cast<size_t>(i) * bytesPerSample, bytesPerSample)) * coeff;
    }
}

inline void SampleConv::FloatToIntBytes(const float* src, int n, char* dest, int bytesPerSample, float scale)
{
    const int q_max = intBytesMax(bytesPerSample);
    const float coeff = scale * static_cast<float>(q_max);
    for (int i = 0; i < n; i++)
    {
        int v = static_cast<int>(std::round(src[i] * coeff));
        if (v < -q_max - 1)
        {
            v = -q_max - 1;
        }
        else if (v > q_max)
        {
            v = q_max;
        }
        writeIntLE(dest + static_cast<size_t>(i) * bytesPerSample, bytesPerSample, v);
    }
}

inline void SampleConv::DoubleToIntBytes(const double* src, int n, char* dest, int bytesPerSample, double scale)
{
    const int q_max = intBytesMax(bytesPerSample);
    const double coeff = scale * static_cast<double>(q_max);
    for (int i = 0; i < n; i++)
    {
        int v = static_cast<int>(std::round(src[i] * coeff));
        if (v < -q_max - 1)
        {
            v = -q_max - 1;
        }
        else if (v > q_max)
        {
            v = q_max;
        }
        writeIntLE(dest + static_cast<size_t>(i) * bytesPerSample, bytesPerSample, v);
    }
}

template<typename DST>
inline void SampleConv::IntBytesToInt(const char* src, int n, DST* dest, int srcBytes)
{
    constexpr int dstDigits = std::numeric_limits<DST>::digits;
    const int srcDigits = srcBytes * 8 - 1;
    const int shift = dstDigits - srcDigits;
    for (int i = 0; i < n; i++)
    {
        int v = readIntLE(src + static_cast<size_t>(i) * srcBytes, srcBytes);
        long long r = (shift >= 0) ? (static_cast<long long>(v) << shift) : (static_cast<long long>(v) >> (-shift));
        dest[i] = static_cast<DST>(r);
    }
}

template<typename SRC>
inline void SampleConv::IntToIntBytes(const SRC* src, int n, char* dest, int dstBytes)
{
    constexpr int srcDigits = std::numeric_limits<SRC>::digits;
    const int dstDigits = dstBytes * 8 - 1;
    const int shift = dstDigits - srcDigits;
    for (int i = 0; i < n; i++)
    {
        long long r = (shift >= 0) ? (static_cast<long long>(src[i]) << shift) : (static_cast<long long>(src[i]) >> (-shift));
        writeIntLE(dest + static_cast<size_t>(i) * dstBytes, dstBytes, static_cast<int>(r));
    }
}

inline void SampleConv::IntBytesToInt24C(const char* src, int n, int* dest, int srcBytes)
{
    const int srcDigits = srcBytes * 8 - 1;
    const int shift = 23 - srcDigits;   //容器语义为24位(值位23)
    for (int i = 0; i < n; i++)
    {
        int v = readIntLE(src + static_cast<size_t>(i) * srcBytes, srcBytes);
        long long r = (shift >= 0) ? (static_cast<long long>(v) << shift) : (static_cast<long long>(v) >> (-shift));
        dest[i] = static_cast<int>(r);
    }
}

inline void SampleConv::Int24CToIntBytes(const int* src, int n, char* dest, int dstBytes)
{
    const int dstDigits = dstBytes * 8 - 1;
    const int shift = dstDigits - 23;
    for (int i = 0; i < n; i++)
    {
        long long r = (shift >= 0) ? (static_cast<long long>(src[i]) << shift) : (static_cast<long long>(src[i]) >> (-shift));
        writeIntLE(dest + static_cast<size_t>(i) * dstBytes, dstBytes, static_cast<int>(r));
    }
}
