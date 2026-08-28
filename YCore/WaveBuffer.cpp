#include "WaveBuffer.h"
#include<limits>

WaveBuffer::WaveBuffer(SampleType type, int sampleLen, int channelNum)
    :_type{type}, _chnNum{ channelNum}
{
    switch (this->_type)
    {
        case SampleType::IEEE64: this->_byteDepth = 8; break;
        case SampleType::IEEE32: this->_byteDepth = 4; break;
        case SampleType::INT32:  this->_byteDepth = 4; break;
        case SampleType::INT24:  this->_byteDepth = 3; break;
        case SampleType::INT16:  this->_byteDepth = 2; break;
    default:
        this->_byteDepth = 4;
        break;
    }

    this->_frameSize = this->_chnNum * this->_byteDepth; //帧大小
    int bufferSize = sampleLen * this->_frameSize;
    this->_pRing = std::make_unique<ByteRing>(bufferSize, this->_frameSize);
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
    int rdSamples = 0;  //读取的采样点数;
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
            auto dest = sample.pf + rdFrames;
            this->toFloat32(src, dest, nFrames, sample._chnInx);
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
    int rdSamples = 0;  //读取的采样点数;
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
            auto src = sample.pf + rdFrames;
            auto dest = buf.data();  //起始指针位置
            this->fromFloat32(src, dest, nFrames, sample._chnInx);
        }

        rdFrames += nFrames; //计算已读帧数
        this->_pRing->releaseReadBuffer();
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

    int rdSamples = 0;  //读取的采样点数;
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

    int rdSamples = 0;  //读取的采样点数;
    int rdFrames = 0;  //读取的帧数

    while(true)
    {
        auto byteSize = (sampleNum - rdFrames) * this->_frameSize;
        auto buf = this->_pRing->getWriteBuffer(byteSize);
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
                //std::copy_n(src, this->_byteDepth, dest);
                std::copy_n(dest, this->_byteDepth, src);
                src += this->_frameSize; //起始指针
                dest += this->_byteDepth;
            }
        }

        rdFrames += nFrames; //计算已读帧数
        this->_pRing->releaseWriteBuffer();
    }

    return rdFrames;
}

int WaveBuffer::writeBytes(char *ptr, int byteSize)
{
    return this->_pRing->write(ptr, byteSize);
}

int WaveBuffer::readBytes(char *ptr, int byteSize)
{
    return this->_pRing->read(ptr, byteSize);
}

