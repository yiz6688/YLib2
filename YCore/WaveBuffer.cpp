#include "WaveBuffer.h"

WaveBuffer::WaveBuffer(int _sampleLen, SampleType _sampleType)
    :sampleLen{ _sampleLen }, sampleType{ _sampleType}
{
    switch (this->sampleType)
    {
        case SampleType::IEEE64: this->ss.pd = new double[sampleLen];
        case SampleType::IEEE32: this->ss.pf = new float[sampleLen];
        case SampleType::INT32:  this->ss.pi = new int[sampleLen];
        case SampleType::INT24:  this->ss.pt = new int24[sampleLen];
        case SampleType::INT16:  this->ss.ps = new short[sampleLen];
    default:
        break;
    }
}

WaveBuffer::~WaveBuffer()
{

    switch (this->sampleType)
    {
        case SampleType::IEEE64: delete[] this->ss.pd; break;
        case SampleType::IEEE32: delete[] this->ss.pf; break;
        case SampleType::INT32:  delete[] this->ss.pi; break;
        case SampleType::INT24:  delete[] this->ss.pt; break;
        case SampleType::INT16:  delete[] this->ss.ps; break;
    default:
        break;
    }
    this->ss.raw = nullptr;  //置空
}