#pragma once
#include"RingBuffer.h"
#include"WaveFormat.h"
#include<memory>


union _sample_
{
	float f32;
	int   i32;
	short i16;
	char  raw;
};


class WaveRingBuffer
{
public:
	WaveRingBuffer(SampleType sampleType, int sampleNum);

	~WaveRingBuffer() = default;


	WaveRingBuffer(WaveRingBuffer&&) = default;
	WaveRingBuffer& operator=(WaveRingBuffer&&) = default;

public:
	int readFloat(float* buffer, int nSample);
	int writeFloat(float* buffer, int nSample);

	int readInt32(int* buffer, int nSample);
	int writeInt32(int* buffer, int nSample);

	int readInt16(short* buffer, int nSample);
	int writeInt16(short* buffer, int nSample);

	int readInt24(int * buffer, int nSample);
	int writeInt24(int* buffer, int nSample);

	int readInt24Bytes(char* buffer, int nSample);
	int writeInt24Bytes(char* buffer, int nSample);

	int writeBytes(char* bytes, int byteSize);
	int readBytes(char* bytes, int byteSize);

	//可读点数
	int getReadableSample();
	//可写点数
	int getWriteableSample();

	int getCapacity();

public:
	SampleType _sampleType;
	int _nBytes;
	std::unique_ptr<RingBuffer> _pRingBuffer;
	
	std::unique_ptr<_sample_[]> _sampleBuffer;
	int _sampleBufferSize;
};