#include "WaveBuffer.h"
#include<limits>
#include<stdexcept>

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

    this->_frameSize = this->_chnNum * this->_byteDepth; //帧大小
    //环形缓冲区内部已通过 _max_size = _cap_aligned - _gap 处理 gap 哨兵,
    //这里直接按 sampleLen * frameSize 申请即可。实际可用帧数为
    //_cap_aligned/_frameSize - 1(恰好对齐 2 的幂时为 sampleLen-1,其余情况 >= sampleLen)
    long long bufferSize64 = static_cast<long long>(sampleLen);
    bufferSize64 *= this->_frameSize;
    if (bufferSize64 <= 0 || bufferSize64 > static_cast<long long>(std::numeric_limits<int>::max()))
    {
        throw std::invalid_argument("缓冲区大小超出范围");
    }
    int bufferSize = static_cast<int>(bufferSize64);
    this->_pRing = std::make_unique<ByteRing>(static_cast<unsigned>(bufferSize), this->_frameSize);
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
                auto dest = sample.pf + rdFrames;
                this->toFloat32(src, dest, nFrames, sample._chnInx);
            }else
            {
                auto dest = sample.pd + rdFrames;
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
                auto src = sample.pf + rdFrames;
                this->fromFloat32(src, dest, nFrames, sample._chnInx);
            }else
            {
                auto src = sample.pd + rdFrames;
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
            auto dest = sample.raw + rdFrames * this->_byteDepth;
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
            auto dest = sample.raw + rdFrames * this->_byteDepth;
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
    if (sample.raw == nullptr || sampleNum <= 0 || sample._type != this->_type)
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
        std::memcpy(sample.raw + static_cast<long>(rdFrames) * this->_frameSize,
            buf.data(), static_cast<size_t>(nFrames) * this->_frameSize);
        rdFrames += nFrames;
        this->_pRing->releaseReadBuffer();
    }
    return rdFrames;
}

//交织原始数据写入: 直接拷贝 sample.raw 交织字节到环形区, 返回写入的帧数
int WaveBuffer::writeRaw(Sample& sample, int sampleNum)
{
    if (sample.raw == nullptr || sampleNum <= 0 || sample._type != this->_type)
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
        std::memcpy(buf.data(), sample.raw + static_cast<long>(wrFrames) * this->_frameSize,
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

