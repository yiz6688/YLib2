#include"WaveWriter.h"
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

WaveWriter::WaveWriter(WaveStream& stream, SampleType storageType)
	: _ptr{ nullptr }, _stream(&stream), _storageType{ storageType }
{
	this->_wb = std::make_unique<WaveBuffer>(this->_storageType,
		this->_chunkFrames + 4, stream.getWaveFormat().getChannels());
}

WaveWriter::WaveWriter(std::unique_ptr<WaveStream>&& stream, SampleType storageType)
	: _ptr{ std::move(stream) }, _stream(_ptr.get()), _storageType{ storageType }
{
	this->_wb = std::make_unique<WaveBuffer>(this->_storageType,
		this->_chunkFrames + 4, this->_stream->getWaveFormat().getChannels());
}

//统一的交织浮点写入: writeFloat 转换进 WaveBuffer 环形区 -> getReadBuffer 锁定排空写流
template<typename F>
int WaveWriter::writeFloatImpl(F* buffer, int sampleNum)
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
		//交织浮点写入 WaveBuffer(转换为存储类型, 进入环形区)
		int w = this->_wb->writeFloat(buffer + doneFloats, sampleNum - static_cast<int>(doneFloats));
		if (w <= 0)
		{
			break;
		}
		//每通道需要排空的字节数
		int remainPerCh = static_cast<int>(w / channels * bytesPerSample);
		//借用 getReadBuffer 锁定直接排空环形区写流, 无需额外缓冲
		while (remainPerCh > 0)
		{
			auto sp = this->_wb->getReadBuffer(remainPerCh);
			if (sp.size() == 0)
			{
				break;
			}
			long wr = this->_stream->write(sp.data(), static_cast<int>(sp.size()));  //失败抛出异常
			this->_wb->releaseReadBuffer();
			int wb = static_cast<int>(wr);
			remainPerCh -= wb / channels;
			doneFloats += wb / bytesPerSample;
		}
	}
	return static_cast<int>(doneFloats);
}

int WaveWriter::writeFloat(float* buffer, int sampleNum)
{
	return this->writeFloatImpl(buffer, sampleNum);
}

int WaveWriter::writeFloat(double* buffer, int sampleNum)
{
	return this->writeFloatImpl(buffer, sampleNum);
}

//原生类型交织写入: 直接写原始字节到 stream
int WaveWriter::writeRaw(Sample& sample, int sampleNum)
{
	if (sample._raw == nullptr || sampleNum <= 0 || sample._type != this->_storageType)
	{
		return 0;
	}
	const int frameSize = this->_stream->getWaveFormat().getBlockAlign();
	long wr = this->_stream->write(sample._raw, sampleNum * frameSize);  //失败抛出异常
	return static_cast<int>(wr) / frameSize;
}

//写入原始字节(透传到底层 WaveStream), 失败抛出异常
long WaveWriter::write(char* buffer, int size, int offset, int count)
{
	return this->_stream->write(buffer, size, offset, count);
}

long WaveWriter::write(char* buffer, int size)
{
	return this->_stream->write(buffer, size);
}

const WaveFormat& WaveWriter::getWaveFormat() const
{
	return this->_stream->getWaveFormat();
}

long WaveWriter::getLength()
{
	return this->_stream->getLength();
}

long WaveWriter::getFrameCount()
{
	return this->_stream->getFrameCount();
}

long WaveWriter::getTotalMills()
{
	return this->_stream->getTotalMills();
}

int WaveWriter::getChannels()
{
	return this->_stream->getChannels();
}

void WaveWriter::flush()
{
	this->_stream->flush();
}

long WaveWriter::getPosition()
{
	return this->_stream->getPosition();
}

long WaveWriter::seek(long offset, SeekOrigin origin)
{
	return this->_stream->seek(offset, origin);
}

long WaveWriter::getTimePos()
{
	return this->_stream->getTimePos();
}

//一次性从文件创建(内部创建并持有 WaveStream), 工厂: 失败转 expected
TPResult<WaveWriter> WaveWriter::create(const WaveFormat& waveFormat, std::string_view filepath)
{
	try
	{
		auto ws = WaveStream::create(waveFormat, filepath);
		if (!ws)
		{
			return make_err<WaveWriter>(ws.error());
		}

		SampleType st = SampleType::UNKNOWN;
		if (!formatToSampleType((*ws)->getWaveFormat(), st) || (*ws)->getWaveFormat().getChannels() <= 0)
		{
			return make_err<WaveWriter>("暂不支持的格式");
		}

		TPtr<WaveWriter> ptr = TPtr<WaveWriter>(new WaveWriter(std::move(ws.value()), st));

		return ptr;
	}
	catch (const std::exception& e)
	{
		return make_err<WaveWriter>(e.what());
	}
}

//借用已存在的 WaveStream(使用权), 工厂: 失败转 expected
TPResult<WaveWriter> WaveWriter::create(WaveStream& stream)
{
	try
	{
		SampleType st = SampleType::UNKNOWN;
		if (!formatToSampleType(stream.getWaveFormat(), st) || stream.getWaveFormat().getChannels() <= 0)
		{
			return make_err<WaveWriter>("暂不支持的格式");
		}

		TPtr<WaveWriter> ptr = TPtr<WaveWriter>(new WaveWriter(stream, st));

		return ptr;
	}
	catch (const std::exception& e)
	{
		return make_err<WaveWriter>(e.what());
	}
}

//转移 WaveStream 所有权, 工厂: 失败转 expected
TPResult<WaveWriter> WaveWriter::create(std::unique_ptr<WaveStream>&& stream)
{
	try
	{
		if (!stream)
		{
			return make_err<WaveWriter>("空 WaveStream");
		}

		SampleType st = SampleType::UNKNOWN;
		if (!formatToSampleType(stream->getWaveFormat(), st) || stream->getWaveFormat().getChannels() <= 0)
		{
			return make_err<WaveWriter>("暂不支持的格式");
		}

		TPtr<WaveWriter> ptr = TPtr<WaveWriter>(new WaveWriter(std::move(stream), st));

		return ptr;
	}
	catch (const std::exception& e)
	{
		return make_err<WaveWriter>(e.what());
	}
}
