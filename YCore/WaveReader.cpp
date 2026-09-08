#include"WaveReader.h"
#include"FileStream.h"
#include<stdexcept>

//根据格式映射到 WaveBuffer 使用的 SampleType
static bool formatToSampleType(const WaveFormat& fmt, SampleType& out)
{
	auto enc = fmt.getEncoding();
	int bits = fmt.getBitsPerSample();
	if (enc == WaveFormatEncoding::IeeeFloat)
	{
		if (bits == 32) { out = SampleType::IEEE32; return true; }
		if (bits == 64) { out = SampleType::IEEE64; return true; }
		return false;
	}
	if (bits == 16) { out = SampleType::INT16; return true; }
	if (bits == 24) { out = SampleType::INT24; return true; }
	if (bits == 32) { out = SampleType::INT32; return true; }
	return false;
}

WaveReader::WaveReader(WaveStream& stream, SampleType storageType)
	: _ptr{ nullptr }, _stream(&stream), _storageType{ storageType }
{
	this->_wb = std::make_unique<WaveBuffer>(this->_storageType,
		this->_chunkFrames + 4, stream.getWaveFormat().getChannels());
}

WaveReader::WaveReader(std::unique_ptr<WaveStream>&& stream, SampleType storageType)
	: _ptr{ std::move(stream) }, _stream(_ptr.get()), _storageType{ storageType }
{
	this->_wb = std::make_unique<WaveBuffer>(this->_storageType,
		this->_chunkFrames + 4, this->_stream->getWaveFormat().getChannels());
}

//统一的交织浮点读取: 从 stream 读原始字节进 WaveBuffer 环形区 -> readFloat 转换输出
template<typename F>
int WaveReader::readFloatImpl(F* buffer, int sampleNum)
{
	if (buffer == nullptr || sampleNum <= 0 || this->_wb == nullptr)
	{
		return 0;
	}
	const int channels = this->_stream->getWaveFormat().getChannels();
	const int frameSize = this->_stream->getWaveFormat().getBlockAlign();
	if (channels <= 0 || frameSize <= 0)
	{
		return 0;
	}
	int bytesPerSample = frameSize / channels;

	long doneFloats = 0;
	while (doneFloats < sampleNum)
	{
		//剩余可读字节(限制在 data 块内)
		long avail = this->_stream->getLength() - this->_stream->getPosition();
		if (avail <= 0)
		{
			break;
		}
		long remain = (sampleNum - doneFloats) * bytesPerSample;
		if (avail > remain)
		{
			avail = remain;
		}
		//借用 getWriteBuffer 锁定环形区, 直接把 stream 数据读入, 用完整体释放
		int perCh = static_cast<int>(avail / channels);
		auto sp = this->_wb->getWriteBuffer(perCh);
		if (sp.size() == 0)
		{
			break;
		}
		long rr = this->_stream->read(sp.data(), static_cast<int>(sp.size()));  //失败抛出异常
		this->_wb->releaseWriteBuffer();
		int gotFloats = static_cast<int>(rr) / bytesPerSample;
		if (gotFloats <= 0)
		{
			break;
		}
		int f = this->_wb->readFloat(buffer + doneFloats, gotFloats);
		if (f <= 0)
		{
			break;
		}
		doneFloats += f;
	}
	return static_cast<int>(doneFloats);
}

int WaveReader::readFloat(float* buffer, int sampleNum)
{
	return this->readFloatImpl(buffer, sampleNum);
}

int WaveReader::readFloat(double* buffer, int sampleNum)
{
	return this->readFloatImpl(buffer, sampleNum);
}

//原生类型交织读取: 直接读原始字节到 sample.raw
int WaveReader::readRaw(Sample& sample, int sampleNum)
{
	if (sample._raw == nullptr || sampleNum <= 0 || sample._type != this->_storageType)
	{
		return 0;
	}
	const int frameSize = this->_stream->getWaveFormat().getBlockAlign();
	long rr = this->_stream->read(sample._raw, sampleNum * frameSize);  //失败抛出异常
	return static_cast<int>(rr) / frameSize;
}

//读取原始字节(透传到底层 WaveStream), 失败抛出异常
long WaveReader::read(char* buffer, int size, int offset, int count)
{
	return this->_stream->read(buffer, size, offset, count);
}

long WaveReader::read(char* buffer, int size)
{
	return this->_stream->read(buffer, size);
}

const WaveFormat& WaveReader::getWaveFormat() const
{
	return this->_stream->getWaveFormat();
}

long WaveReader::getLength()
{
	return this->_stream->getLength();
}

long WaveReader::getFrameCount()
{
	return this->_stream->getFrameCount();
}

long WaveReader::getTotalMills()
{
	return this->_stream->getTotalMills();
}

int WaveReader::getChannels()
{
	return this->_stream->getChannels();
}

long WaveReader::getPosition()
{
	return this->_stream->getPosition();
}

long WaveReader::seek(long offset, SeekOrigin origin)
{
	return this->_stream->seek(offset, origin);
}

long WaveReader::getTimePos()
{
	return this->_stream->getTimePos();
}

//一次性从文件创建(内部创建并持有 WaveStream), 工厂: 失败转 expected
TPResult<WaveReader> WaveReader::open(std::string_view filepath)
{
	try
	{
		auto ws = WaveStream::open(filepath);
		if (!ws)
		{
			return make_err<WaveReader>(ws.error());
		}

		SampleType st = SampleType::UNKNOWN;
		if (!formatToSampleType((*ws)->getWaveFormat(), st) || (*ws)->getWaveFormat().getChannels() <= 0)
		{
			return make_err<WaveReader>("暂不支持的格式");
		}

		TPtr<WaveReader> ptr = TPtr<WaveReader>(new WaveReader(std::move(ws.value()), st));

		return ptr;
	}
	catch (const std::exception& e)
	{
		return make_err<WaveReader>(e.what());
	}
}

//借用已存在的 WaveStream(使用权), 工厂: 失败转 expected
TPResult<WaveReader> WaveReader::open(WaveStream& stream)
{
	try
	{
		SampleType st = SampleType::UNKNOWN;
		if (!formatToSampleType(stream.getWaveFormat(), st) || stream.getWaveFormat().getChannels() <= 0)
		{
			return make_err<WaveReader>("暂不支持的格式");
		}

		TPtr<WaveReader> ptr = TPtr<WaveReader>(new WaveReader(stream, st));

		return ptr;
	}
	catch (const std::exception& e)
	{
		return make_err<WaveReader>(e.what());
	}
}

//转移 WaveStream 所有权, 工厂: 失败转 expected
TPResult<WaveReader> WaveReader::open(std::unique_ptr<WaveStream>&& stream)
{
	try
	{
		if (!stream)
		{
			return make_err<WaveReader>("空 WaveStream");
		}

		SampleType st = SampleType::UNKNOWN;
		if (!formatToSampleType(stream->getWaveFormat(), st) || stream->getWaveFormat().getChannels() <= 0)
		{
			return make_err<WaveReader>("暂不支持的格式");
		}

		TPtr<WaveReader> ptr = TPtr<WaveReader>(new WaveReader(std::move(stream), st));

		return ptr;
	}
	catch (const std::exception& e)
	{
		return make_err<WaveReader>(e.what());
	}
}
