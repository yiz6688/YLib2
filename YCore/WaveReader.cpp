#include<memory>
#include"BitConverter.h"
#include"BinaryStream.h"
#include"SampleConv.h"
#include<chrono>
#include "WaveReader.h"
#include"FileStream.h"

using namespace std;

const int rf64ChunkId = BitConverter::Converter<int>("RF64").value();
const int riffChunkId = BitConverter::Converter<int>("RIFF").value();
const int waveChunkId = BitConverter::Converter<int>("WAVE").value();
const int dataChunkId = BitConverter::Converter<int>("data").value();
const int formatChunkId = BitConverter::Converter<int>("fmt ").value();

//检查返回值如果失败返回错误
#define CHECK_RESULT(result) if (!result) { return std::unexpected{ result.error() }; }

class WaveChunkReader
{

public:
	WaveChunkReader()
	{
	}

	~WaveChunkReader()
	{
	}


	expected<void, string> ReadWaveHeader(Stream* stream)
	{
		auto result = stream->getPosition();
		if (!result)
		{
			return std::unexpected(result.error());
		}
		long position = result.value(); //当前所在位置
		
		result = stream->getLength();
		if(!result)
		{
			return std::unexpected(result.error());
		}
		long size = result.value(); //流总长度
		long remainSize = size - position; //剩余长度



		bool fmtchunkFlag = false;  //是否读取到format chunk的标志
		BinaryStream bs(stream);
		
		//检查RIFF头
		auto riffID = bs.readInt32();
		if (!riffID)
		{
			return std::unexpected(riffID.error());
		}
		if (riffID.value() != riffChunkId)
		{
			return unexpected("Not a WAVE file - no RIFF header");
		}

		auto riffSize = bs.readInt32();
		if (!riffSize)
		{
			return std::unexpected(riffSize.error());
		}
		this->_riffSize = riffSize.value();

		//检查WAVE头
		auto waveID = bs.readInt32();
		if (!waveID)
		{
			return unexpected(waveID.error());
		}
		if (waveID.value() != waveChunkId)
		{
			return std::unexpected("Not a WAVE file - no WAVE header");
		}

		long endpos = std::min<long>(this->_riffSize + 8, remainSize);  //取两者小的


		result = stream->getPosition();
		if (!result)
		{
			return std::unexpected(result.error());
		}
		position = result.value();

		while (position  <= endpos - 8)
		{
			result = bs.readInt32();
			if (!result)
			{
				return std::unexpected(result.error());
			}
			long chunkID = result.value();

			result = bs.readInt32();
			if (!result)
			{
				return std::unexpected(result.error());
			}
			long chunkSize = result.value();

			if (chunkID == dataChunkId)
			{
				this->_dataPos = position;
				//this->_dataSize = chunkSize;
				this->_dataSize = std::min<long>(chunkSize, size - position);  //如果chunkSize超过剩余长度，说明文件损坏了，取剩余长度

				if (fmtchunkFlag == false)
				{
					return std::unexpected("Invalid WAV file - fmt chunk must be before data chunk");
				}

				auto nn = this->_fmt->getBlockAlign();
				this->_dataSize -= (this->_dataSize % nn);  //对齐采样块
				

				//移动指针到下一个chunk
				result = stream->seek(chunkSize, SeekOrigin::Current);
				if (!result)
				{
					return std::unexpected(result.error());
				}
				position = result.value();
			}
			else if (chunkID == formatChunkId)
			{
				if (chunkSize > INT32_MAX)
				{
					return std::unexpected("Format chunk length must be between 0 and intmax");
				}
				//获取waveformat,这里不用移动了,下面的读取会自动移动指针
				this->_fmt = WaveFormat::fromFormatChunk(*stream, chunkSize); 
				fmtchunkFlag = true;
				result = stream->getPosition();
				if (!result)
				{
					return std::unexpected(result.error());
				}
				position = result.value();
				continue;
			}
			else
			{
				if (chunkSize > size - position)
				{
					//如果chunkSize超过剩余长度，说明文件损坏了
					break;
				}
				
				if (chunkSize > INT32_MAX)
				{
					return std::unexpected("RIFFChunk chunk length must be between 0 and intmax");
				}
				this->_riffLst.emplace_back(RIFFChunk{ chunkID, chunkSize, position });
				//移动指针到下一个chunk
				result = stream->seek(chunkSize, SeekOrigin::Current);
				if (!result)
				{
					return std::unexpected(result.error());
				}
				position = result.value();
			}

		}


		if (fmtchunkFlag == false)
		{
			return std::unexpected("Invalid WAV file - No fmt chunk found");
		}

		if (this->_dataPos == 0)
		{
			return std::unexpected("Invalid WAV file - No data chunk found");
		}

		return {};
	}

public:

	std::unique_ptr<WaveFormat> _fmt;
	
	long _riffSize{ 0 };

	long _dataPos{ 0 };

	long _dataSize{ 0 };

	vector<RIFFChunk> _riffLst{};
};



WaveReader::WaveReader(Stream* stream)
	:_ptr{nullptr}, _stream(stream),  _dataPos{0}, _dataSize{0}
{
	auto result = this->readWaveHeader();
	if (!result)
	{
		throw std::runtime_error(result.error());
	}
}

WaveReader::WaveReader(const std::string & filePath)
	:_ptr{ new FileStream(filePath, FileMode::Open, FileAccess::Read) }, _stream(_ptr.get())
	, _dataPos{ 0 }, _dataSize{ 0 }
{

	auto result = this->readWaveHeader();
	if (!result)
	{
		throw std::runtime_error(result.error());
	}
}

WaveReader::~WaveReader()
{}

std::vector<RIFFChunk>& WaveReader::getExtraChunks()
{
	return this->_extraChunks;
}

std::vector<char> WaveReader::getChunkData(RIFFChunk chunk)
{
	std::vector<char> data(chunk.size);
	auto result = this->setPosition(chunk.offset);  //后续处理，先按异常抛
	if (!result)
	{
		throw std::runtime_error(result.error());
	}
	auto ret = this->_stream->read(data);
	if (!ret)
	{
		throw std::runtime_error(ret.error());
	}
	return data;
}

std::expected<long, std::string> WaveReader::getPosition()
{
	return this->_stream->getPosition().value() - this->_dataPos - 8;
}

std::expected<void, std::string> WaveReader::setPosition(long value)
{
	auto len = this->_stream->getLength();

	if (value > len.value())
	{
		value = len.value();
	}
	value -= (value % this->_fmt->getBlockAlign());  //对齐采样块

	return this->_stream->setPosition(value + this->_dataPos + 8);
}

std::expected<long, std::string> WaveReader::seek(long offset, SeekOrigin origin)
{
	auto pos = this->_stream->getPosition();
	auto len = this->_stream->getLength();

	if (origin == SeekOrigin::Current)
	{
		offset += pos.value();
	}
	else if (origin == SeekOrigin::End)
	{
		offset = len.value() + offset;
	}

	auto result = this->setPosition(offset); CHECK_RESULT(result);

	return this->getPosition();
}

std::expected<long, std::string> WaveReader::seekTime(long mills, SeekOrigin origin)
{
	auto bytes = this->_fmt->mills2Bytes(mills);
	return this->seek(bytes, origin);
}

std::expected<void, std::string> WaveReader::setTimePos(long mills)
{
	long bytes = this->_fmt->mills2Bytes(mills);
	return this->setPosition(bytes);
}

std::expected<long, std::string> WaveReader::getTimePos()
{
	auto pos = this->getPosition();
	if (!pos)
	{
		return pos;
	}
	return this->_fmt->bytes2Mills(pos.value());
}

std::expected<long, std::string> WaveReader::readSamples(float* buffer, int nsamples)
{
	int nBytes = this->_fmt->getBlockAlign() / this->_fmt->getChannels();
	int nret = 0;
	int rdLen = 0;
	int start = 0;
	long readSamples = 0;
	if (this->_fmt->getEncoding() == WaveFormatEncoding::IeeeFloat)
	{
		char* ptr = reinterpret_cast<char*>(buffer);
		nret = this->_stream->read(ptr, nsamples * 4).value();   //指向调用，预防子类重写后循环调用
		readSamples = nret / 4;
		return readSamples;
	}

	int nSample = 0;
	if (nBytes == 2)
	{
		float* ptr = buffer;
		auto len = this->bufferLen / 2;  //缓冲区有效长度
		short* sptr = reinterpret_cast<short*>(this->convBuffer);  //转换缓冲区
		rdLen = len;
		for (start = 0; start < nsamples; start += len)
		{
			ptr = buffer + readSamples;
			if (start + rdLen >= nsamples)
			{
				rdLen = nsamples - start;
			}

			auto result = this->_stream->read(this->convBuffer, rdLen * 2);   //指向调用，预防子类重写后循环调用
			CHECK_RESULT(result);
			nSample = result.value() / 2;
			SampleConv::Int16toFloat(sptr, nSample, ptr);
			
			readSamples += nSample;
		};
		return readSamples;
	}
	else if (nBytes == 3)
	{
		float* ptr = buffer;
		auto len = this->bufferLen / 3;  //缓冲区有效长度
		rdLen = len;
		for (start = 0; start < nsamples; start += len)
		{
			ptr = buffer + readSamples;
			if (start + rdLen >= nsamples)
			{
				rdLen = nsamples - start;
			}

			auto result = this->_stream->read(this->convBuffer, rdLen * 3);   //指向调用，预防子类重写后循环调用
			CHECK_RESULT(result);
			nSample = result.value() / 3;
			SampleConv::Int24BytetoFloat(this->convBuffer, nSample, ptr);

			readSamples += result.value() / 3;
		};
		return readSamples;
	}
	else if (nBytes == 4)
	{
		float* ptr = buffer;
		auto len = this->bufferLen / 4;  //缓冲区有效长度
		int* iptr = reinterpret_cast<int*>(this->convBuffer);  //转换缓冲区
		rdLen = len;
		for (start = 0; start < nsamples; start += len)
		{
			ptr = buffer + readSamples;
			if (start + rdLen >= nsamples)
			{
				rdLen = nsamples - start;
			}

			auto result = this->_stream->read(this->convBuffer, rdLen * 4);   //指向调用，预防子类重写后循环调用
			CHECK_RESULT(result);
			nSample = result.value() / 4;
			SampleConv::Int32toFloat(iptr, nSample, ptr);

			readSamples += nSample;
		};
		return readSamples;
	}
	else
	{
		throw std::logic_error("暂不支持的格式");
	}
	return 0;

}

std::expected<long, std::string> WaveReader::readSamples64(double *buffer, int nsamples)
{
	int nBytes = this->_fmt->getBlockAlign() / this->_fmt->getChannels();
	int nret = 0;
	int rdLen = 0;
	int start = 0;
	long readSamples = 0;
	
	int nSample = 0;
	if (nBytes == 2)
	{
		double* ptr = buffer;
		auto len = this->bufferLen / 2;  //缓冲区有效长度
		short* sptr = reinterpret_cast<short*>(this->convBuffer);  //转换缓冲区
		rdLen = len;
		for (start = 0; start < nsamples; start += len)
		{
			ptr = buffer + readSamples;
			if (start + rdLen >= nsamples)
			{
				rdLen = nsamples - start;
			}

			auto result = this->_stream->read(this->convBuffer, rdLen * 2);   //指向调用，预防子类重写后循环调用
			CHECK_RESULT(result);
			nSample = result.value() / 2;
			SampleConv::IntToDouble(sptr, nSample, ptr);
			
			readSamples += nSample;
		};
		return readSamples;
	}
	else if (nBytes == 4)
	{
		double* ptr = buffer;
		auto len = this->bufferLen / 4;  //缓冲区有效长度
		int* iptr = reinterpret_cast<int*>(this->convBuffer);  //转换缓冲区
		rdLen = len;
		for (start = 0; start < nsamples; start += len)
		{
			ptr = buffer + readSamples;
			if (start + rdLen >= nsamples)
			{
				rdLen = nsamples - start;
			}

			auto result = this->_stream->read(this->convBuffer, rdLen * 4);   //指向调用，预防子类重写后循环调用
			CHECK_RESULT(result);
			nSample = result.value() / 4;
			SampleConv::IntToDouble(iptr, nSample, ptr);

			readSamples += nSample;
		};
		return readSamples;
	}
	else
	{
		throw std::logic_error("暂不支持的格式");
	}
	return 0;

}

std::expected<long, std::string>  WaveReader::read(char* buffer, int size)
{
	return this->read(buffer, size, 0, size);
}

std::expected<long, std::string>  WaveReader::read(char* buffer, int size, int offset, int count)
{
	auto result = this->getPosition(); CHECK_RESULT(result);
	auto value = this->_dataSize - result.value();
	if (value <= 0)
	{
		return 0;
	}
	
	if (value > count)
	{
		value = count;
	}

	return this->_stream->read(buffer + offset, value);
}

const WaveFormat& WaveReader::getWaveFormat() const
{
	return *(this->_fmt);
}

long WaveReader::getLength()
{
	return this->_dataSize;
}

long WaveReader::getFrameCount()
{
	return this->_dataSize / this->_fmt->getBlockAlign();
}

long WaveReader::getTotalMills()
{
	return this->_dataSize * 1000 / this->_fmt->getBytesPerSec();
}

std::expected<void, std::string> WaveReader::readWaveHeader()
{
	WaveChunkReader chunkReader;
	auto result = chunkReader.ReadWaveHeader(this->_stream);
	CHECK_RESULT(result);

	this->_fmt = std::move(chunkReader._fmt);
	this->_dataPos = chunkReader._dataPos;
	this->_dataSize = chunkReader._dataSize;
	this->_extraChunks = std::move(chunkReader._riffLst);
	result = this->setPosition(0);  //设置到wav起始位置
	CHECK_RESULT(result);

	return {};
}
