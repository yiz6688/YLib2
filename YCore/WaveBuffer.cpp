#include "WaveBuffer.h"
#include<limits>
#include<stdexcept>
#include<bit>
#include<cmath>
#include<algorithm>
#include<cstring>

WaveBuffer::WaveBuffer(SampleType type, int sampleLen, int channelNum)
    :_type{type}, _chnNum{ channelNum}
{
    if (sampleLen <= 0)
    {
        throw std::invalid_argument("sampleLen 必须大于 0");
    }
    if (channelNum <= 0)
    {
        throw std::invalid_argument("channelNum 必须大于 0");
    }

    switch (this->_type)
    {
        case SampleType::IEEE64: this->_byteDepth = 8; break;
        case SampleType::IEEE32: this->_byteDepth = 4; break;
        case SampleType::INT32:  this->_byteDepth = 4; break;
        case SampleType::INT24:  this->_byteDepth = 3; break;
        case SampleType::INT16:  this->_byteDepth = 2; break;
        case SampleType::INT64:  this->_byteDepth = 8; break;
    default:
        this->_byteDepth = 4;
        break;
    }

    this->_frameSize = this->_chnNum * this->_byteDepth; //帧大小(字节, 可为任意值如 INT24=3)
    //按帧管理: 帧数量取 2 的幂次(至少 sampleLen+1, 保留一帧哨兵), 门限精确限制为 sampleLen 帧
    if (static_cast<long long>(sampleLen) + 1 > (1LL << 31))
    {
        throw std::invalid_argument("缓冲区帧数超出范围");
    }
    unsigned frameCount = std::bit_ceil(static_cast<unsigned>(sampleLen) + 1);
    this->_pRing = std::make_unique<ByteRing>(frameCount, static_cast<unsigned>(this->_frameSize));
    this->_pRing->setLimit(static_cast<unsigned>(sampleLen)); //门限: 有效可用空间恰好 sampleLen 帧
}

WaveBuffer::~WaveBuffer()
{
}

int WaveBuffer::readSample(WaveMix &mix, int sampleNum)
{
    if(mix._type != SampleType::IEEE32 && mix._type != SampleType::IEEE64)
    {
        return 0;
    }

    int rdFrames = 0;  //读取的帧数
    while(true)
    {
        auto byteSize = (sampleNum - rdFrames) * this->_frameSize;
        auto buf = this->_pRing->getReadBuffer(byteSize);
        if(buf.size() == 0)
        {
            break;
        }

        int nFrames = buf.size() / this->_frameSize;  //读取的帧数
        auto& samples = mix._datas;

        for(auto& sample : samples)
        {
            auto src = buf.data();  //起始指针位置
            if(mix._type == SampleType::IEEE32)
            {
                auto dest = reinterpret_cast<float*>(sample._raw) + rdFrames;
                this->toFloat32(src, dest, nFrames, sample._chnInx);
            }else
            {
                auto dest = reinterpret_cast<double*>(sample._raw) + rdFrames;
                this->toFloat32(src, dest, nFrames, sample._chnInx);
            }
        }
        rdFrames += nFrames; //计算已读帧数
        this->_pRing->releaseReadBuffer();
    }

    return rdFrames;
}

int WaveBuffer::writeSample(WaveMix &mix, int sampleNum)
{
    if(mix._type != SampleType::IEEE32 && mix._type != SampleType::IEEE64)
    {
        return 0;
    }

    int rdFrames = 0;  //已写入的帧数
    while(true)
    {
        auto byteSize = (sampleNum - rdFrames) * this->_frameSize;
        auto buf = this->_pRing->getWriteBuffer(byteSize);
        if(buf.size() == 0)
        {
            break;
        }

        int nFrames = buf.size() / this->_frameSize;  //可写入的帧数
        auto& samples = mix._datas;

        for(auto& sample : samples)
        {
            auto dest = buf.data();  //起始指针位置
            //源采样类型由 mix 决定(浮点/双精度),而不是缓冲区存储类型
            if(mix._type == SampleType::IEEE32)
            {
                auto src = reinterpret_cast<float*>(sample._raw) + rdFrames;
                this->fromFloat32(src, dest, nFrames, sample._chnInx);
            }else
            {
                auto src = reinterpret_cast<double*>(sample._raw) + rdFrames;
                this->fromFloat32(src, dest, nFrames, sample._chnInx);
            }
        }
        rdFrames += nFrames; //计算已写帧数
        this->_pRing->releaseWriteBuffer();
    }

    return rdFrames;
}

// 按帧进行读取
int WaveBuffer::readRaw(WaveMix &mix, int sampleNum)
{
    if(this->_type != mix._type)
    {
        return 0;  //不进行读取
    }

    int rdFrames = 0;  //读取的帧数


    while(true)
    {
        auto byteSize = (sampleNum - rdFrames) * this->_frameSize;
        auto buf = this->_pRing->getReadBuffer(byteSize);
        if(buf.size() == 0)
        {
            break;
        }

        int nFrames = buf.size() / this->_frameSize;  //读取的帧数

        auto& samples = mix._datas;

        for(auto& sample : samples)
        {
            auto src = buf.data() + sample._chnInx * this->_byteDepth;  //起始指针位置
            auto dest = sample._raw + rdFrames * this->_byteDepth;
            for(int i=0; i<nFrames; i++)
            {
                std::copy_n(src, this->_byteDepth, dest);
                src += this->_frameSize; //起始指针
                dest += this->_byteDepth;
            }
        }

        rdFrames += nFrames; //计算已读帧数
        this->_pRing->releaseReadBuffer();
    }

    return rdFrames;
}

int WaveBuffer::writeRaw(WaveMix &mix, int sampleNum)
{
    if(this->_type != mix._type)
    {
        return 0;  //不进行读取
    }

    int rdFrames = 0;  //已写入的帧数

    while(true)
    {
        auto byteSize = (sampleNum - rdFrames) * this->_frameSize;
        auto buf = this->_pRing->getWriteBuffer(byteSize);
        if(buf.size() == 0)
        {
            break;
        }

        int nFrames = buf.size() / this->_frameSize;  //可写入的帧数

        auto& samples = mix._datas;

        for(auto& sample : samples)
        {
            auto src = buf.data() + sample._chnInx * this->_byteDepth;  //起始指针位置
            auto dest = sample._raw + rdFrames * this->_byteDepth;
            for(int i=0; i<nFrames; i++)
            {
                std::copy_n(dest, this->_byteDepth, src);
                src += this->_frameSize; //起始指针
                dest += this->_byteDepth;
            }
        }

        rdFrames += nFrames; //计算已写帧数
        this->_pRing->releaseWriteBuffer();
    }

    return rdFrames;
}

int WaveBuffer::writeBytes(char *ptr, int byteSize)
{
    int size = (byteSize / this->_frameSize) * this->_frameSize;
    return this->_pRing->write(ptr, size);
}

int WaveBuffer::readBytes(char *ptr, int byteSize)
{
    int size = (byteSize / this->_frameSize) * this->_frameSize;
    return this->_pRing->read(ptr, size);
}

//锁定读取缓冲(perChSize 为单通道字节数): 单通道按位宽对齐后乘通道数得到交织总字节
std::span<char> WaveBuffer::getReadBuffer(int perChSize)
{
    int size = (perChSize / this->_byteDepth) * this->_byteDepth;
    size *= this->_chnNum;
    return this->_pRing->getReadBuffer(size);
}

int WaveBuffer::releaseReadBuffer()
{
    return this->_pRing->releaseReadBuffer();
}

//锁定写入缓冲(perChSize 为单通道字节数): 单通道按位宽对齐后乘通道数得到交织总字节
std::span<char> WaveBuffer::getWriteBuffer(int perChSize)
{
    int size = (perChSize / this->_byteDepth) * this->_byteDepth;
    size *= this->_chnNum;
    return this->_pRing->getWriteBuffer(size);
}

int WaveBuffer::releaseWriteBuffer()
{
    return this->_pRing->releaseWriteBuffer();
}

//交织原始数据读取: 直接拷贝环形区交织字节到 sample.raw, 返回读出的帧数
int WaveBuffer::readRaw(Sample& sample, int sampleNum)
{
    if (sample._raw == nullptr || sampleNum <= 0 || sample._type != this->_type)
    {
        return 0;
    }
    int rdFrames = 0;
    while (rdFrames < sampleNum)
    {
        int byteSize = (sampleNum - rdFrames) * this->_frameSize;
        auto buf = this->_pRing->getReadBuffer(byteSize);
        if (buf.size() == 0)
        {
            break;
        }
        int nFrames = buf.size() / this->_frameSize;
        std::memcpy(sample._raw + static_cast<long>(rdFrames) * this->_frameSize,
            buf.data(), static_cast<size_t>(nFrames) * this->_frameSize);
        rdFrames += nFrames;
        this->_pRing->releaseReadBuffer();
    }
    return rdFrames;
}

//交织原始数据写入: 直接拷贝 sample.raw 交织字节到环形区, 返回写入的帧数
int WaveBuffer::writeRaw(Sample& sample, int sampleNum)
{
    if (sample._raw == nullptr || sampleNum <= 0 || sample._type != this->_type)
    {
        return 0;
    }
    int wrFrames = 0;
    while (wrFrames < sampleNum)
    {
        int byteSize = (sampleNum - wrFrames) * this->_frameSize;
        auto buf = this->_pRing->getWriteBuffer(byteSize);
        if (buf.size() == 0)
        {
            break;
        }
        int nFrames = buf.size() / this->_frameSize;
        std::memcpy(buf.data(), sample._raw + static_cast<long>(wrFrames) * this->_frameSize,
            static_cast<size_t>(nFrames) * this->_frameSize);
        wrFrames += nFrames;
        this->_pRing->releaseWriteBuffer();
    }
    return wrFrames;
}

//纯字节单通道去交织读取: 环形区 ch 通道的字节拷到 dest(连续), byteSize 为该通道字节数
int WaveBuffer::readChannelBytes(int ch, char* dest, int byteSize)
{
    if (ch < 0 || ch >= this->_chnNum || dest == nullptr || byteSize <= 0)
    {
        return 0;
    }
    long wantBytes = static_cast<long>(byteSize) / this->_byteDepth * this->_byteDepth; //向下对齐到位宽
    if (wantBytes <= 0)
    {
        return 0;
    }
    long done = 0;
    while (done < wantBytes)
    {
        int needFrames = static_cast<int>((wantBytes - done) / this->_byteDepth);
        auto buf = this->_pRing->getReadBuffer(needFrames * this->_frameSize);
        if (buf.size() == 0)
        {
            break;
        }
        int nFrames = static_cast<int>(buf.size() / this->_frameSize);
        const char* src = buf.data() + static_cast<long>(ch) * this->_byteDepth;
        char* d = dest + done;
        for (int i = 0; i < nFrames; i++)
        {
            std::memcpy(d + static_cast<long>(i) * this->_byteDepth,
                src + static_cast<long>(i) * this->_frameSize, this->_byteDepth);
        }
        done += static_cast<long>(nFrames) * this->_byteDepth;
        this->_pRing->releaseReadBuffer();
    }
    return static_cast<int>(done);
}

//纯字节单通道交织写入: src 的该通道字节拷回环形区 ch 通道, byteSize 为该通道字节数
int WaveBuffer::writeChannelBytes(int ch, const char* src, int byteSize)
{
    if (ch < 0 || ch >= this->_chnNum || src == nullptr || byteSize <= 0)
    {
        return 0;
    }
    long wantBytes = static_cast<long>(byteSize) / this->_byteDepth * this->_byteDepth; //向下对齐到位宽
    if (wantBytes <= 0)
    {
        return 0;
    }
    long done = 0;
    while (done < wantBytes)
    {
        int needFrames = static_cast<int>((wantBytes - done) / this->_byteDepth);
        auto buf = this->_pRing->getWriteBuffer(needFrames * this->_frameSize);
        if (buf.size() == 0)
        {
            break;
        }
        int nFrames = static_cast<int>(buf.size() / this->_frameSize);
        char* dst = buf.data() + static_cast<long>(ch) * this->_byteDepth;
        const char* s = src + done;
        for (int i = 0; i < nFrames; i++)
        {
            std::memcpy(dst + static_cast<long>(i) * this->_frameSize,
                s + static_cast<long>(i) * this->_byteDepth, this->_byteDepth);
        }
        done += static_cast<long>(nFrames) * this->_byteDepth;
        this->_pRing->releaseWriteBuffer();
    }
    return static_cast<int>(done);
}

//纯字节按需多通道去交织读取: 一次锁定按指定通道读入各自缓冲(各 dest 得到相同字节数), 返回每通道字节数
int WaveBuffer::readChannelsBytes(const std::vector<ChannelBytes>& channels)
{
    if (channels.empty())
    {
        return 0;
    }
    //校验: 任一通道非法则整体失败; 取各通道字节数(向下对齐)的最小值
    long minBytes = std::numeric_limits<long>::max();
    for (const auto& c : channels)
    {
        if (c.chnInx < 0 || c.chnInx >= this->_chnNum || c.raw == nullptr || c.byteSize <= 0)
        {
            return 0;
        }
        long aligned = static_cast<long>(c.byteSize) / this->_byteDepth * this->_byteDepth;
        if (aligned < minBytes)
        {
            minBytes = aligned;
        }
    }
    if (minBytes <= 0)
    {
        return 0;
    }

    long done = 0;
    while (done < minBytes)
    {
        int needFrames = static_cast<int>((minBytes - done) / this->_byteDepth);
        auto buf = this->_pRing->getReadBuffer(needFrames * this->_frameSize);
        if (buf.size() == 0)
        {
            break;
        }
        int nFrames = static_cast<int>(buf.size() / this->_frameSize);
        for (const auto& c : channels)
        {
            const char* src = buf.data() + static_cast<long>(c.chnInx) * this->_byteDepth;
            char* d = c.raw + done;
            for (int i = 0; i < nFrames; i++)
            {
                std::memcpy(d + static_cast<long>(i) * this->_byteDepth,
                    src + static_cast<long>(i) * this->_frameSize, this->_byteDepth);
            }
        }
        done += static_cast<long>(nFrames) * this->_byteDepth;
        this->_pRing->releaseReadBuffer();
    }
    return static_cast<int>(done);
}

//纯字节按需多通道交织写入: 一次锁定按指定通道写入各自缓冲(各 src 需提供相同字节数), 返回每通道字节数
int WaveBuffer::writeChannelsBytes(const std::vector<ChannelBytes>& channels)
{
    if (channels.empty())
    {
        return 0;
    }
    //校验: 任一通道非法则整体失败; 取各通道字节数(向下对齐)的最小值
    long minBytes = std::numeric_limits<long>::max();
    for (const auto& c : channels)
    {
        if (c.chnInx < 0 || c.chnInx >= this->_chnNum || c.raw == nullptr || c.byteSize <= 0)
        {
            return 0;
        }
        long aligned = static_cast<long>(c.byteSize) / this->_byteDepth * this->_byteDepth;
        if (aligned < minBytes)
        {
            minBytes = aligned;
        }
    }
    if (minBytes <= 0)
    {
        return 0;
    }

    long done = 0;
    while (done < minBytes)
    {
        int needFrames = static_cast<int>((minBytes - done) / this->_byteDepth);
        auto buf = this->_pRing->getWriteBuffer(needFrames * this->_frameSize);
        if (buf.size() == 0)
        {
            break;
        }
        int nFrames = static_cast<int>(buf.size() / this->_frameSize);
        for (const auto& c : channels)
        {
            char* dst = buf.data() + static_cast<long>(c.chnInx) * this->_byteDepth;
            const char* s = c.raw + done;
            for (int i = 0; i < nFrames; i++)
            {
                std::memcpy(dst + static_cast<long>(i) * this->_frameSize,
                    s + static_cast<long>(i) * this->_byteDepth, this->_byteDepth);
            }
        }
        done += static_cast<long>(nFrames) * this->_byteDepth;
        this->_pRing->releaseWriteBuffer();
    }
    return static_cast<int>(done);
}

//=============================================================================
// 交织整型/原始字节读写(类似 WaveRingBuffer, 内部格式自动转换)
// 说明: nSample 为总采样数(交织, = 帧数 × 通道数); 返回实际读写的采样数
//=============================================================================

int WaveBuffer::readInt16(short* buffer, int nSample)
{
    if (buffer == nullptr || nSample <= 0 || this->_chnNum <= 0)
    {
        return 0;
    }
    int frames = nSample / this->_chnNum;
    if (frames <= 0)
    {
        return 0;
    }
    int rdFrames = 0;
    while (rdFrames < frames)
    {
        int byteSize = (frames - rdFrames) * this->_frameSize;
        auto buf = this->_pRing->getReadBuffer(byteSize);
        if (buf.size() == 0)
        {
            break;
        }
        int nFrames = buf.size() / this->_frameSize;
        for (int ch = 0; ch < this->_chnNum; ch++)
        {
            this->toInt16(buf.data(), buffer + static_cast<long>(rdFrames) * this->_chnNum + ch,
                nFrames, ch, this->_chnNum);
        }
        rdFrames += nFrames;
        this->_pRing->releaseReadBuffer();
    }
    return rdFrames * this->_chnNum;
}

int WaveBuffer::writeInt16(short* buffer, int nSample)
{
    if (buffer == nullptr || nSample <= 0 || this->_chnNum <= 0)
    {
        return 0;
    }
    int frames = nSample / this->_chnNum;
    if (frames <= 0)
    {
        return 0;
    }
    int wrFrames = 0;
    while (wrFrames < frames)
    {
        int byteSize = (frames - wrFrames) * this->_frameSize;
        auto buf = this->_pRing->getWriteBuffer(byteSize);
        if (buf.size() == 0)
        {
            break;
        }
        int nFrames = buf.size() / this->_frameSize;
        for (int ch = 0; ch < this->_chnNum; ch++)
        {
            this->fromInt16(buffer + static_cast<long>(wrFrames) * this->_chnNum + ch,
                buf.data(), nFrames, ch, this->_chnNum);
        }
        wrFrames += nFrames;
        this->_pRing->releaseWriteBuffer();
    }
    return wrFrames * this->_chnNum;
}

int WaveBuffer::readInt24(int* buffer, int nSample)
{
    if (buffer == nullptr || nSample <= 0 || this->_chnNum <= 0)
    {
        return 0;
    }
    int frames = nSample / this->_chnNum;
    if (frames <= 0)
    {
        return 0;
    }
    int rdFrames = 0;
    while (rdFrames < frames)
    {
        int byteSize = (frames - rdFrames) * this->_frameSize;
        auto buf = this->_pRing->getReadBuffer(byteSize);
        if (buf.size() == 0)
        {
            break;
        }
        int nFrames = buf.size() / this->_frameSize;
        for (int ch = 0; ch < this->_chnNum; ch++)
        {
            this->toInt24(buf.data(), buffer + static_cast<long>(rdFrames) * this->_chnNum + ch,
                nFrames, ch, this->_chnNum);
        }
        rdFrames += nFrames;
        this->_pRing->releaseReadBuffer();
    }
    return rdFrames * this->_chnNum;
}

int WaveBuffer::writeInt24(int* buffer, int nSample)
{
    if (buffer == nullptr || nSample <= 0 || this->_chnNum <= 0)
    {
        return 0;
    }
    int frames = nSample / this->_chnNum;
    if (frames <= 0)
    {
        return 0;
    }
    int wrFrames = 0;
    while (wrFrames < frames)
    {
        int byteSize = (frames - wrFrames) * this->_frameSize;
        auto buf = this->_pRing->getWriteBuffer(byteSize);
        if (buf.size() == 0)
        {
            break;
        }
        int nFrames = buf.size() / this->_frameSize;
        for (int ch = 0; ch < this->_chnNum; ch++)
        {
            this->fromInt24(buffer + static_cast<long>(wrFrames) * this->_chnNum + ch,
                buf.data(), nFrames, ch, this->_chnNum);
        }
        wrFrames += nFrames;
        this->_pRing->releaseWriteBuffer();
    }
    return wrFrames * this->_chnNum;
}

int WaveBuffer::readInt32(int* buffer, int nSample)
{
    if (buffer == nullptr || nSample <= 0 || this->_chnNum <= 0)
    {
        return 0;
    }
    int frames = nSample / this->_chnNum;
    if (frames <= 0)
    {
        return 0;
    }
    int rdFrames = 0;
    while (rdFrames < frames)
    {
        int byteSize = (frames - rdFrames) * this->_frameSize;
        auto buf = this->_pRing->getReadBuffer(byteSize);
        if (buf.size() == 0)
        {
            break;
        }
        int nFrames = buf.size() / this->_frameSize;
        for (int ch = 0; ch < this->_chnNum; ch++)
        {
            this->toInt32(buf.data(), buffer + static_cast<long>(rdFrames) * this->_chnNum + ch,
                nFrames, ch, this->_chnNum);
        }
        rdFrames += nFrames;
        this->_pRing->releaseReadBuffer();
    }
    return rdFrames * this->_chnNum;
}

int WaveBuffer::writeInt32(int* buffer, int nSample)
{
    if (buffer == nullptr || nSample <= 0 || this->_chnNum <= 0)
    {
        return 0;
    }
    int frames = nSample / this->_chnNum;
    if (frames <= 0)
    {
        return 0;
    }
    int wrFrames = 0;
    while (wrFrames < frames)
    {
        int byteSize = (frames - wrFrames) * this->_frameSize;
        auto buf = this->_pRing->getWriteBuffer(byteSize);
        if (buf.size() == 0)
        {
            break;
        }
        int nFrames = buf.size() / this->_frameSize;
        for (int ch = 0; ch < this->_chnNum; ch++)
        {
            this->fromInt32(buffer + static_cast<long>(wrFrames) * this->_chnNum + ch,
                buf.data(), nFrames, ch, this->_chnNum);
        }
        wrFrames += nFrames;
        this->_pRing->releaseWriteBuffer();
    }
    return wrFrames * this->_chnNum;
}

int WaveBuffer::readInt24Bytes(char* buffer, int nSample)
{
    if (buffer == nullptr || nSample <= 0 || this->_chnNum <= 0)
    {
        return 0;
    }
    int frames = nSample / this->_chnNum;
    if (frames <= 0)
    {
        return 0;
    }
    int stride = this->_chnNum * 3;   //交织24位, 每帧字节数
    int rdFrames = 0;
    while (rdFrames < frames)
    {
        int byteSize = (frames - rdFrames) * this->_frameSize;
        auto buf = this->_pRing->getReadBuffer(byteSize);
        if (buf.size() == 0)
        {
            break;
        }
        int nFrames = buf.size() / this->_frameSize;
        for (int ch = 0; ch < this->_chnNum; ch++)
        {
            this->toInt24Bytes(buf.data(), buffer + static_cast<long>(rdFrames) * stride,
                nFrames, ch, stride);
        }
        rdFrames += nFrames;
        this->_pRing->releaseReadBuffer();
    }
    return rdFrames * this->_chnNum;
}

int WaveBuffer::writeInt24Bytes(char* buffer, int nSample)
{
    if (buffer == nullptr || nSample <= 0 || this->_chnNum <= 0)
    {
        return 0;
    }
    int frames = nSample / this->_chnNum;
    if (frames <= 0)
    {
        return 0;
    }
    int stride = this->_chnNum * 3;   //交织24位, 每帧字节数
    int wrFrames = 0;
    while (wrFrames < frames)
    {
        int byteSize = (frames - wrFrames) * this->_frameSize;
        auto buf = this->_pRing->getWriteBuffer(byteSize);
        if (buf.size() == 0)
        {
            break;
        }
        int nFrames = buf.size() / this->_frameSize;
        for (int ch = 0; ch < this->_chnNum; ch++)
        {
            this->fromInt24Bytes(buffer + static_cast<long>(wrFrames) * stride,
                buf.data(), nFrames, ch, stride);
        }
        wrFrames += nFrames;
        this->_pRing->releaseWriteBuffer();
    }
    return wrFrames * this->_chnNum;
}

int WaveBuffer::getReadableSample()
{
    return static_cast<int>(this->_pRing->getReadableFrames() * this->_chnNum);
}

int WaveBuffer::getWriteableSample()
{
    return static_cast<int>(this->_pRing->getWriteableFrames() * this->_chnNum);
}

int WaveBuffer::getCapacity()
{
    return static_cast<int>(this->_pRing->getMaxFrames() * this->_chnNum);
}

//=============================================================================
// 内部格式 <-> 目标整型 逐通道转换
//=============================================================================

void WaveBuffer::toInt16(char* src, short* dest, int nFrames, int chnIndex, int stride)
{
    src = src + chnIndex * this->_byteDepth;
    switch (this->_type)
    {
    case SampleType::INT16:
        for (int i = 0; i < nFrames; i++)
        {
            short v = 0;
            std::memcpy(&v, src, 2);
            *dest = v;
            src += this->_frameSize;
            dest += stride;
        }
        break;
    case SampleType::INT24:
        for (int i = 0; i < nFrames; i++)
        {
            int v = ((src[0] & 0xFF) << 8) | ((src[1] & 0xFF) << 16) | ((src[2] & 0xFF) << 24);
            v >>= 8;   //24位有符号 -> int16
            *dest = static_cast<short>(v);
            src += this->_frameSize;
            dest += stride;
        }
        break;
    case SampleType::INT32:
        for (int i = 0; i < nFrames; i++)
        {
            int v = 0;
            std::memcpy(&v, src, 4);
            *dest = static_cast<short>(v >> 16);
            src += this->_frameSize;
            dest += stride;
        }
        break;
    case SampleType::IEEE32:
        for (int i = 0; i < nFrames; i++)
        {
            float v = 0;
            std::memcpy(&v, src, 4);
            *dest = static_cast<short>(std::round(std::clamp(v, -1.0f, 1.0f) * 32767.0f));
            src += this->_frameSize;
            dest += stride;
        }
        break;
    case SampleType::IEEE64:
        for (int i = 0; i < nFrames; i++)
        {
            double v = 0;
            std::memcpy(&v, src, 8);
            *dest = static_cast<short>(std::round(std::clamp(v, -1.0, 1.0) * 32767.0));
            src += this->_frameSize;
            dest += stride;
        }
        break;
    default:
        break;
    }
}

void WaveBuffer::fromInt16(short* src, char* dest, int nFrames, int chnIndex, int stride)
{
    dest = dest + chnIndex * this->_byteDepth;
    switch (this->_type)
    {
    case SampleType::INT16:
        for (int i = 0; i < nFrames; i++)
        {
            std::memcpy(dest, src, 2);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    case SampleType::INT24:
        for (int i = 0; i < nFrames; i++)
        {
            int v = *src;
            dest[0] = static_cast<char>(v & 0xFF);
            dest[1] = static_cast<char>((v >> 8) & 0xFF);
            dest[2] = static_cast<char>((v >> 16) & 0xFF);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    case SampleType::INT32:
        for (int i = 0; i < nFrames; i++)
        {
            int v = static_cast<int>(*src) << 16;
            std::memcpy(dest, &v, 4);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    case SampleType::IEEE32:
        for (int i = 0; i < nFrames; i++)
        {
            float v = static_cast<float>(*src) / 32768.0f;
            std::memcpy(dest, &v, 4);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    case SampleType::IEEE64:
        for (int i = 0; i < nFrames; i++)
        {
            double v = static_cast<double>(*src) / 32768.0;
            std::memcpy(dest, &v, 8);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    default:
        break;
    }
}

void WaveBuffer::toInt24(char* src, int* dest, int nFrames, int chnIndex, int stride)
{
    src = src + chnIndex * this->_byteDepth;
    switch (this->_type)
    {
    case SampleType::INT24:
        for (int i = 0; i < nFrames; i++)
        {
            int v = ((src[0] & 0xFF) << 8) | ((src[1] & 0xFF) << 16) | ((src[2] & 0xFF) << 24);
            v >>= 8;   //24位有符号
            *dest = v;
            src += this->_frameSize;
            dest += stride;
        }
        break;
    case SampleType::INT16:
        for (int i = 0; i < nFrames; i++)
        {
            short v = 0;
            std::memcpy(&v, src, 2);
            *dest = static_cast<int>(v) << 8;
            src += this->_frameSize;
            dest += stride;
        }
        break;
    case SampleType::INT32:
        for (int i = 0; i < nFrames; i++)
        {
            int v = 0;
            std::memcpy(&v, src, 4);
            *dest = v >> 8;
            src += this->_frameSize;
            dest += stride;
        }
        break;
    case SampleType::IEEE32:
        for (int i = 0; i < nFrames; i++)
        {
            float v = 0;
            std::memcpy(&v, src, 4);
            *dest = static_cast<int>(std::round(std::clamp(v, -1.0f, 1.0f) * 8388607.0f));
            src += this->_frameSize;
            dest += stride;
        }
        break;
    case SampleType::IEEE64:
        for (int i = 0; i < nFrames; i++)
        {
            double v = 0;
            std::memcpy(&v, src, 8);
            *dest = static_cast<int>(std::round(std::clamp(v, -1.0, 1.0) * 8388607.0));
            src += this->_frameSize;
            dest += stride;
        }
        break;
    default:
        break;
    }
}

void WaveBuffer::fromInt24(int* src, char* dest, int nFrames, int chnIndex, int stride)
{
    dest = dest + chnIndex * this->_byteDepth;
    switch (this->_type)
    {
    case SampleType::INT24:
        for (int i = 0; i < nFrames; i++)
        {
            int v = *src;
            dest[0] = static_cast<char>(v & 0xFF);
            dest[1] = static_cast<char>((v >> 8) & 0xFF);
            dest[2] = static_cast<char>((v >> 16) & 0xFF);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    case SampleType::INT16:
        for (int i = 0; i < nFrames; i++)
        {
            short v = static_cast<short>(*src >> 8);
            std::memcpy(dest, &v, 2);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    case SampleType::INT32:
        for (int i = 0; i < nFrames; i++)
        {
            int v = *src << 8;
            std::memcpy(dest, &v, 4);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    case SampleType::IEEE32:
        for (int i = 0; i < nFrames; i++)
        {
            float v = static_cast<float>(*src) / 8388608.0f;
            std::memcpy(dest, &v, 4);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    case SampleType::IEEE64:
        for (int i = 0; i < nFrames; i++)
        {
            double v = static_cast<double>(*src) / 8388608.0;
            std::memcpy(dest, &v, 8);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    default:
        break;
    }
}

void WaveBuffer::toInt32(char* src, int* dest, int nFrames, int chnIndex, int stride)
{
    src = src + chnIndex * this->_byteDepth;
    switch (this->_type)
    {
    case SampleType::INT32:
        for (int i = 0; i < nFrames; i++)
        {
            int v = 0;
            std::memcpy(&v, src, 4);
            *dest = v;
            src += this->_frameSize;
            dest += stride;
        }
        break;
    case SampleType::INT24:
        for (int i = 0; i < nFrames; i++)
        {
            int v = ((src[0] & 0xFF) << 8) | ((src[1] & 0xFF) << 16) | ((src[2] & 0xFF) << 24);
            v >>= 8;
            *dest = v << 8;
            src += this->_frameSize;
            dest += stride;
        }
        break;
    case SampleType::INT16:
        for (int i = 0; i < nFrames; i++)
        {
            short v = 0;
            std::memcpy(&v, src, 2);
            *dest = static_cast<int>(v) << 16;
            src += this->_frameSize;
            dest += stride;
        }
        break;
    case SampleType::IEEE32:
        for (int i = 0; i < nFrames; i++)
        {
            float v = 0;
            std::memcpy(&v, src, 4);
            *dest = static_cast<int>(std::round(std::clamp(static_cast<double>(v), -1.0, 1.0) * 2147483647.0));
            src += this->_frameSize;
            dest += stride;
        }
        break;
    case SampleType::IEEE64:
        for (int i = 0; i < nFrames; i++)
        {
            double v = 0;
            std::memcpy(&v, src, 8);
            *dest = static_cast<int>(std::round(std::clamp(v, -1.0, 1.0) * 2147483647.0));
            src += this->_frameSize;
            dest += stride;
        }
        break;
    default:
        break;
    }
}

void WaveBuffer::fromInt32(int* src, char* dest, int nFrames, int chnIndex, int stride)
{
    dest = dest + chnIndex * this->_byteDepth;
    switch (this->_type)
    {
    case SampleType::INT32:
        for (int i = 0; i < nFrames; i++)
        {
            std::memcpy(dest, src, 4);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    case SampleType::INT24:
        for (int i = 0; i < nFrames; i++)
        {
            int v = *src >> 8;
            dest[0] = static_cast<char>(v & 0xFF);
            dest[1] = static_cast<char>((v >> 8) & 0xFF);
            dest[2] = static_cast<char>((v >> 16) & 0xFF);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    case SampleType::INT16:
        for (int i = 0; i < nFrames; i++)
        {
            short v = static_cast<short>(*src >> 16);
            std::memcpy(dest, &v, 2);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    case SampleType::IEEE32:
        for (int i = 0; i < nFrames; i++)
        {
            float v = static_cast<float>(*src) / 2147483648.0f;
            std::memcpy(dest, &v, 4);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    case SampleType::IEEE64:
        for (int i = 0; i < nFrames; i++)
        {
            double v = static_cast<double>(*src) / 2147483648.0;
            std::memcpy(dest, &v, 8);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    default:
        break;
    }
}

void WaveBuffer::toInt24Bytes(char* src, char* dest, int nFrames, int chnIndex, int stride)
{
    src = src + chnIndex * this->_byteDepth;
    dest = dest + chnIndex * 3;
    switch (this->_type)
    {
    case SampleType::INT24:
        for (int i = 0; i < nFrames; i++)
        {
            dest[0] = src[0];
            dest[1] = src[1];
            dest[2] = src[2];
            src += this->_frameSize;
            dest += stride;
        }
        break;
    case SampleType::INT16:
        for (int i = 0; i < nFrames; i++)
        {
            short v = 0;
            std::memcpy(&v, src, 2);
            int value = static_cast<int>(v) << 8;
            dest[0] = static_cast<char>(value & 0xFF);
            dest[1] = static_cast<char>((value >> 8) & 0xFF);
            dest[2] = static_cast<char>((value >> 16) & 0xFF);
            src += this->_frameSize;
            dest += stride;
        }
        break;
    case SampleType::INT32:
        for (int i = 0; i < nFrames; i++)
        {
            int v = 0;
            std::memcpy(&v, src, 4);
            int value = v >> 8;
            dest[0] = static_cast<char>(value & 0xFF);
            dest[1] = static_cast<char>((value >> 8) & 0xFF);
            dest[2] = static_cast<char>((value >> 16) & 0xFF);
            src += this->_frameSize;
            dest += stride;
        }
        break;
    case SampleType::IEEE32:
        for (int i = 0; i < nFrames; i++)
        {
            float v = 0;
            std::memcpy(&v, src, 4);
            int value = static_cast<int>(std::round(std::clamp(v, -1.0f, 1.0f) * 8388607.0f));
            dest[0] = static_cast<char>(value & 0xFF);
            dest[1] = static_cast<char>((value >> 8) & 0xFF);
            dest[2] = static_cast<char>((value >> 16) & 0xFF);
            src += this->_frameSize;
            dest += stride;
        }
        break;
    case SampleType::IEEE64:
        for (int i = 0; i < nFrames; i++)
        {
            double v = 0;
            std::memcpy(&v, src, 8);
            int value = static_cast<int>(std::round(std::clamp(v, -1.0, 1.0) * 8388607.0));
            dest[0] = static_cast<char>(value & 0xFF);
            dest[1] = static_cast<char>((value >> 8) & 0xFF);
            dest[2] = static_cast<char>((value >> 16) & 0xFF);
            src += this->_frameSize;
            dest += stride;
        }
        break;
    default:
        break;
    }
}

void WaveBuffer::fromInt24Bytes(char* src, char* dest, int nFrames, int chnIndex, int stride)
{
    src = src + chnIndex * 3;
    dest = dest + chnIndex * this->_byteDepth;
    switch (this->_type)
    {
    case SampleType::INT24:
        for (int i = 0; i < nFrames; i++)
        {
            dest[0] = src[0];
            dest[1] = src[1];
            dest[2] = src[2];
            src += stride;
            dest += this->_frameSize;
        }
        break;
    case SampleType::INT16:
        for (int i = 0; i < nFrames; i++)
        {
            int v = ((src[0] & 0xFF) << 8) | ((src[1] & 0xFF) << 16) | ((src[2] & 0xFF) << 24);
            v >>= 8;
            short s = static_cast<short>(v >> 8);
            std::memcpy(dest, &s, 2);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    case SampleType::INT32:
        for (int i = 0; i < nFrames; i++)
        {
            int v = ((src[0] & 0xFF) << 8) | ((src[1] & 0xFF) << 16) | ((src[2] & 0xFF) << 24);
            v >>= 8;
            int value = v << 8;
            std::memcpy(dest, &value, 4);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    case SampleType::IEEE32:
        for (int i = 0; i < nFrames; i++)
        {
            int v = ((src[0] & 0xFF) << 8) | ((src[1] & 0xFF) << 16) | ((src[2] & 0xFF) << 24);
            v >>= 8;
            float f = static_cast<float>(v) / 8388608.0f;
            std::memcpy(dest, &f, 4);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    case SampleType::IEEE64:
        for (int i = 0; i < nFrames; i++)
        {
            int v = ((src[0] & 0xFF) << 8) | ((src[1] & 0xFF) << 16) | ((src[2] & 0xFF) << 24);
            v >>= 8;
            double d = static_cast<double>(v) / 8388608.0;
            std::memcpy(dest, &d, 8);
            src += stride;
            dest += this->_frameSize;
        }
        break;
    default:
        break;
    }
}


