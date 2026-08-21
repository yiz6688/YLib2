#pragma once

#include"../../RingBuffer.h"
#include"../../WaveFormat.h"
#include<memory>

struct ASIOBuffer
{
 
public:
    ASIOBuffer(int _channel, SampleType _sampleType, int deviceBufferSize, int ringBufferSize)
        :channel{_channel}, sampleType(_sampleType)
    {
        this->bitDepth = 4;
        if (this->sampleType == SampleType::INT16)
        {
            this->bitDepth = 2;
        }
        else if (this->sampleType == SampleType::INT24)
        {
            this->bitDepth = 3;
        }
        else if (this->sampleType == SampleType::INT32)
        {
            this->bitDepth = 4;
        }
        else if (this->sampleType == SampleType::IEEE32)
        {
            this->bitDepth = 4;
        }
        this->deviceByteSize = deviceBufferSize * this->bitDepth;
        pRingBuffer = std::make_unique<RingBuffer>(ringBufferSize * this->bitDepth);
    }


    void inputProcess(int index)
    {
        void* buffer = this->buffers[index];  
        char* ptr = static_cast<char*>(buffer);  
        this->pRingBuffer->write(ptr, this->deviceByteSize);
    }

    void outputProcess(int index)
    {
        auto* buffer = this->buffers[index];
        char* ptr = static_cast<char*>(buffer);
        std::fill_n(ptr, this->deviceByteSize, 0); //清空缓冲区
        this->pRingBuffer->read((char*)ptr, this->deviceByteSize);
    }


    //通道号
    int channel;

    //采样类型
    SampleType sampleType;

    int bitDepth;

    int deviceByteSize;

    std::unique_ptr<RingBuffer> pRingBuffer;

    void* buffers[2] {nullptr, nullptr};
};