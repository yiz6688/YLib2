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
