#include "WaveRingBuffer.h"
#include"SampleConv.h"

WaveRingBuffer::WaveRingBuffer(SampleType sampleType, int sampleNum)
{
	this->_sampleType = sampleType;
	if (this->_sampleType == SampleType::INT16)
	{
		this->_nBytes = 2;
	}
	else if (this->_sampleType == SampleType::INT24)
	{
		this->_nBytes = 3;
	}
	else if (this->_sampleType == SampleType::INT32)
	{
		this->_nBytes = 4;
	}
	else if (this->_sampleType == SampleType::IEEE32)
	{
		this->_nBytes = 4;
	}
	else
	{
		this->_nBytes = 4;
	}

	int bufferSize = sampleNum * this->_nBytes;
	this->_pRingBuffer = std::unique_ptr<RingBuffer>(new RingBuffer(bufferSize));
	this->_sampleBufferSize = 1024;
	this->_sampleBuffer = std::unique_ptr<_sample_[]>(new _sample_[this->_sampleBufferSize]);
}

int WaveRingBuffer::readFloat(float* buffer, int nSample)
{
	int bytesNum = 0;
	int offset = 0;

	if (this->_sampleType == SampleType::IEEE32)
	{
		bytesNum = nSample * this->_nBytes;
		auto* ptr = reinterpret_cast<char*>(buffer);
		int readSize = this->_pRingBuffer->read(ptr, bytesNum);
		return readSize / this->_nBytes;
	}
	else
	{
		auto* sampleBuffer = this->_sampleBuffer.get();
		int readNum = 0;
		int sampleSize = this->_sampleBufferSize * sizeof(_sample_) / this->_nBytes; //计算出来对齐后的采样点数量

		int nReadSample = 0;//可写的采样点数
		while (true)
		{
			nReadSample = nSample - offset;
			if (nReadSample <= 0)
			{
				break;
			}

			if (nReadSample > sampleSize)
			{
				nReadSample = sampleSize;
			}

			readNum = nReadSample * this->_nBytes;
			int readSize = this->_pRingBuffer->read(&sampleBuffer->raw, readNum);
			if (readSize == 0)
			{
				break;
			}
			else if (readSize < readNum)
			{
				break;
			}
			nReadSample = readSize / this->_nBytes;
			if (this->_sampleType == SampleType::INT32)
			{
				SampleConv::Int32toFloat(&sampleBuffer->i32, nReadSample, buffer + offset);
			}
			else if (this->_sampleType == SampleType::INT24)
			{
				SampleConv::Int24BytetoFloat(&sampleBuffer->raw, nReadSample, buffer + offset);
			}
			else if (this->_sampleType == SampleType::INT16)
			{
				SampleConv::Int16toFloat(&sampleBuffer->i16, nReadSample, buffer + offset);
			}

			offset += readSize / this->_nBytes;
		}
	}

	return offset;
}

int WaveRingBuffer::writeFloat(float* buffer, int nSample)
{
	int bytesNum = 0;
	int offset = 0;
	if (this->_sampleType == SampleType::IEEE32)
	{
		bytesNum = nSample * this->_nBytes;
		auto* ptr = reinterpret_cast<char*>(buffer);
		int writeBytes = this->_pRingBuffer->write(ptr, bytesNum);
		return writeBytes / this->_nBytes;
	}
	else
	{
		auto* sampleBuffer = this->_sampleBuffer.get();
		int writeNum = 0;
		int sampleSize = this->_sampleBufferSize * sizeof(_sample_) / this->_nBytes; //计算出来对齐后的采样点数量

		int nWriteSample = 0;//可写的采样点数
		while (true)
		{
			nWriteSample = nSample - offset;
			if (nWriteSample <= 0)
			{
				break;
			}

			if (nWriteSample > sampleSize)
			{
				nWriteSample = sampleSize;
			}
			
			if (this->_sampleType == SampleType::INT32)
			{
				SampleConv::FloattoInt32(buffer+offset, nWriteSample, &sampleBuffer->i32);
			}
			else if (this->_sampleType == SampleType::INT24)
			{
				SampleConv::FloattoInt24Byte(buffer+offset, nWriteSample, &sampleBuffer->raw);
			}
			else if (this->_sampleType == SampleType::INT16)
			{
				SampleConv::FloattoInt16(buffer+offset, nWriteSample, &sampleBuffer->i16);
			}
			writeNum = nWriteSample * this->_nBytes;
			int nWriteBytes = this->_pRingBuffer->write(&sampleBuffer->raw, writeNum);
			offset += nWriteBytes / this->_nBytes;
			if (nWriteBytes == 0)
			{
				break;
			}
			else if (nWriteBytes < writeNum)
			{
				break; //剩余空间不足了
			}
		}
	}

	return offset;
}

int WaveRingBuffer::readInt32(int* buffer, int nSample)
{
	int bytesNum = 0;
	int offset = 0;

	if (this->_sampleType == SampleType::INT32)
	{
		bytesNum = nSample * this->_nBytes;
		auto* ptr = reinterpret_cast<char*>(buffer);
		int readSize = this->_pRingBuffer->read(ptr, bytesNum);
		return readSize / this->_nBytes;
	}
	else
	{
		auto* sampleBuffer = this->_sampleBuffer.get();
		int readNum = 0;
		int nReadSample = 0;//可写的采样点数

		while (true)
		{
			nReadSample = nSample - offset;
			if (nReadSample <= 0)
			{
				break;
			}

			if (nReadSample > this->_sampleBufferSize)
			{
				nReadSample = this->_sampleBufferSize;
			}

			readNum = this->readFloat(&sampleBuffer->f32, nReadSample);
			if (readNum == 0)
			{
				break;
			}
			else if (readNum < nReadSample)
			{
				break;
			}

			SampleConv::FloattoInt32(&sampleBuffer->f32, readNum, buffer + offset);
			offset += readNum;
		}
	}
	return offset;
}

int WaveRingBuffer::writeInt32(int* buffer, int nSample)
{
	int bytesNum = 0;
	int offset = 0;
	if (this->_sampleType == SampleType::INT32)
	{
		bytesNum = nSample * this->_nBytes;
		auto* ptr = reinterpret_cast<char*>(buffer);
		int writeBytes = this->_pRingBuffer->write(ptr, bytesNum);
		return writeBytes / this->_nBytes;
	}
	else
	{
		auto* sampleBuffer = this->_sampleBuffer.get();
		int writeNum = 0;
		int nWriteSample = 0;//可写的采样点数

		while (true)
		{
			nWriteSample = nSample - offset;
			if (nWriteSample <= 0)
			{
				break;
			}

			if (nWriteSample > this->_sampleBufferSize)
			{
				nWriteSample = this->_sampleBufferSize;
			}


			SampleConv::Int32toFloat(buffer + offset, nWriteSample, &sampleBuffer->f32);

			writeNum = this->writeFloat(&sampleBuffer->f32, nWriteSample);
			offset += writeNum;
			if (writeNum == 0)
			{
				break;
			}
			else if (writeNum < nWriteSample)
			{
				break;
			}

			
		}
	}

	return offset;
}

int WaveRingBuffer::readInt16(short* buffer, int nSample)
{
	int bytesNum = 0;
	int offset = 0;

	if (this->_sampleType == SampleType::INT16)
	{
		bytesNum = nSample * this->_nBytes;
		auto* ptr = reinterpret_cast<char*>(buffer);
		int readSize = this->_pRingBuffer->read(ptr, bytesNum);
		return readSize / this->_nBytes;
	}
	else
	{
		auto* sampleBuffer = this->_sampleBuffer.get();
		int readNum = 0;
		int nReadSample = 0;//可写的采样点数

		while (true)
		{
			nReadSample = nSample - offset;
			if (nReadSample <= 0)
			{
				break;
			}

			if (nReadSample > this->_sampleBufferSize)
			{
				nReadSample = this->_sampleBufferSize;
			}

			readNum = this->readFloat(&sampleBuffer->f32, nReadSample);
			
			if (readNum == 0)
			{
				break;
			}
			else if (readNum < nReadSample)
			{
				break;
			}

			SampleConv::FloattoInt16(&sampleBuffer->f32, readNum, buffer + offset);
			offset += readNum;
		}
	}
	return offset;
}

int WaveRingBuffer::writeInt16(short* buffer, int nSample)
{
	int bytesNum = 0;
	int offset = 0;
	if (this->_sampleType == SampleType::INT16)
	{
		bytesNum = nSample * this->_nBytes;
		auto* ptr = reinterpret_cast<char*>(buffer);
		int writeBytes = this->_pRingBuffer->write(ptr, bytesNum);
		return writeBytes / this->_nBytes;
	}
	else
	{
		auto* sampleBuffer = this->_sampleBuffer.get();
		int writeNum = 0;
		int nWriteSample = 0;//可写的采样点数

		while (true)
		{
			nWriteSample = nSample - offset;
			if (nWriteSample <= 0)
			{
				break;
			}

			if (nWriteSample > this->_sampleBufferSize)
			{
				nWriteSample = this->_sampleBufferSize;
			}


			SampleConv::Int16toFloat(buffer + offset, nWriteSample, &sampleBuffer->f32);

			writeNum = this->writeFloat(&sampleBuffer->f32, nWriteSample);
			offset += writeNum;
			if (writeNum == 0)
			{
				break;
			}
			else if (writeNum < nWriteSample)
			{
				break;
			}


		}
	}

	return offset;
}

int WaveRingBuffer::readInt24(int* buffer, int nSample)
{
	int bytesNum = 0;
	int offset = 0;

	
	auto* sampleBuffer = this->_sampleBuffer.get();
	int readNum = 0;
	int nReadSample = 0;//可写的采样点数

	while (true)
	{
		nReadSample = nSample - offset;
		if (nReadSample <= 0)
		{
			break;
		}

		if (nReadSample > this->_sampleBufferSize)
		{
			nReadSample = this->_sampleBufferSize;
		}

		readNum = this->readFloat(&sampleBuffer->f32, nReadSample);
		
		if (readNum == 0)
		{
			break;
		}
		else if (readNum < nReadSample)
		{
			break;
		}

		SampleConv::FloatToInt24(&sampleBuffer->f32, readNum, buffer + offset);
		offset += readNum;
	}
	
	return offset;
}

int WaveRingBuffer::writeInt24(int* buffer, int nSample)
{
	int bytesNum = 0;
	int offset = 0;
	
	auto* sampleBuffer = this->_sampleBuffer.get();
	int writeNum = 0;
	int nWriteSample = 0;//可写的采样点数

	while (true)
	{
		nWriteSample = nSample - offset;
		if (nWriteSample <= 0)
		{
			break;
		}

		if (nWriteSample > this->_sampleBufferSize)
		{
			nWriteSample = this->_sampleBufferSize;
		}


		SampleConv::Int24ToFloat(buffer + offset, nWriteSample, &sampleBuffer->f32);

		writeNum = this->writeFloat(&sampleBuffer->f32, nWriteSample);
		offset += writeNum;
		if (writeNum == 0)
		{
			break;
		}
		else if (writeNum < nWriteSample)
		{
			break;
		}


	}
	

	return offset;
}

int WaveRingBuffer::readInt24Bytes(char* buffer, int nSample)
{
	int bytesNum = 0;
	int offset = 0;

	if (this->_sampleType == SampleType::INT24)
	{
		bytesNum = nSample * this->_nBytes;
		int readSize = this->_pRingBuffer->read(buffer, bytesNum);
		return readSize / this->_nBytes;
	}
	else
	{
		auto* sampleBuffer = this->_sampleBuffer.get();
		int readNum = 0;
		int nReadSample = 0;//可写的采样点数

		while (true)
		{
			nReadSample = nSample - offset;
			if (nReadSample <= 0)
			{
				break;
			}

			if (nReadSample > this->_sampleBufferSize)
			{
				nReadSample = this->_sampleBufferSize;
			}

			readNum = this->readFloat(&sampleBuffer->f32, nReadSample);
			
			if (readNum == 0)
			{
				break;
			}
			else if (readNum < nReadSample)
			{
				break;
			}

			SampleConv::FloattoInt24Byte(&sampleBuffer->f32, readNum, buffer + offset * 3);
			offset += readNum;
		}
	}
	return offset;
}

int WaveRingBuffer::writeInt24Bytes(char* buffer, int nSample)
{
	int bytesNum = 0;
	int offset = 0;
	if (this->_sampleType == SampleType::INT24)
	{
		bytesNum = nSample * this->_nBytes;
		int writeBytes = this->_pRingBuffer->write(buffer, bytesNum);
		return writeBytes / this->_nBytes;
	}
	else
	{
		auto* sampleBuffer = this->_sampleBuffer.get();
		int writeNum = 0;
		int nWriteSample = 0;//可写的采样点数

		while (true)
		{
			nWriteSample = nSample - offset;
			if (nWriteSample <= 0)
			{
				break;
			}

			if (nWriteSample > this->_sampleBufferSize)
			{
				nWriteSample = this->_sampleBufferSize;
			}


			SampleConv::Int24BytetoFloat(buffer + offset * 3, nWriteSample, &sampleBuffer->f32);

			writeNum = this->writeFloat(&sampleBuffer->f32, nWriteSample);
			offset += writeNum;
			if (writeNum == 0)
			{
				break;
			}
			else if (writeNum < nWriteSample)
			{
				break;
			}


		}
	}

	return offset;
}

int WaveRingBuffer::writeBytes(char* bytes, int byteSize)
{
	return this->_pRingBuffer->write(bytes, byteSize);
}

int WaveRingBuffer::readBytes(char* bytes, int byteSize)
{
	return this->_pRingBuffer->read(bytes, byteSize);
}

int WaveRingBuffer::getReadableSample()
{
	int nReadableBytes = this->_pRingBuffer->getReadableBytes();
	return nReadableBytes / this->_nBytes;
}

int WaveRingBuffer::getWriteableSample()
{
	int nWriteableBytes = this->_pRingBuffer->getWriteableBytes();
	return nWriteableBytes / this->_nBytes;
}

int WaveRingBuffer::getCapacity()
{
	return this->_pRingBuffer->getCapacity();
}
