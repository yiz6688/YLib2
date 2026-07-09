#include"WaveWriter.h"
#include"FileStream.h"


//检查返回值如果失败返回错误
#define CHECK_RESULT(result) if (!result) { return std::unexpected{ result.error() }; }



WaveWriter::WaveWriter(const WaveFormat& _waveFormat, Stream* stream)
	:_ptr(nullptr), _stream(stream), _dataSize(0), _fmt{ _waveFormat.clone() }
{
	auto result = this->writeWaveHeader();
	if(! result)
	{
		throw std::runtime_error(result.error());
	}

}

WaveWriter::WaveWriter(const WaveFormat& _waveFormat, const std::string& filePath)
	:_ptr{new FileStream(filePath, FileMode::Create, FileAccess::Write)},
	_stream{ _ptr.get() }, _dataSize{ 0 }, _fmt{ _waveFormat.clone() }
{
	auto result = this->writeWaveHeader();
	if (!result)
	{
		throw std::runtime_error(result.error());
	}
}


WaveWriter::~WaveWriter()
{
	//this->flush(); 
};

std::expected<long, std::string> WaveWriter::write(char* buffer, int size, int offset, int count)
{

	if (count % this->_fmt->getBlockAlign() != 0)
	{
		return std::unexpected{ "写入块没有对齐" };
	}
	auto result = this->_stream->write(buffer, size, offset, count); CHECK_RESULT(result);
	this->_dataSize += result.value();  //更新data块数量
	auto ret = this->updateHeader();   CHECK_RESULT(ret);  //刷新写入数据后更新头部信息

	return result.value();
}

std::expected<long, std::string> WaveWriter::write(char* buffer, int count)
{
	return this->write(buffer, count, 0, count);
}

std::expected<long, std::string> WaveWriter::writeSamples(float* buffer, int nsamples)
{

	int nBytes = this->_fmt->getBitsPerSample() / 8;
	int start = 0;
	int wlen = 0;
	long writeSamples = 0;
	if (this->_fmt->getEncoding() == WaveFormatEncoding::IeeeFloat)
	{
		char* ptr = reinterpret_cast<char*>(buffer);
		auto result = this->write(ptr, nsamples * 4);//浮点数不需要转换直接写入
		CHECK_RESULT(result);
		writeSamples = (result.value() / 4);
		return writeSamples;
	}

	if (nBytes == 2)
	{
		auto len = this->bufferLen / 2;  //缓冲区有效长度
		short* sptr = reinterpret_cast<short*>(this->convBuffer);  //转换缓冲区
		wlen = len;

		for (start = 0; start < nsamples; start += len)
		{
			if (start + wlen >= nsamples)
			{
				wlen = nsamples - start;
			}
			SampleConv::FloattoInt16(buffer + start, wlen, sptr);  //每次转换这么长
			auto result = this->write(this->convBuffer, wlen * 2);  //转换过的数据写入
			CHECK_RESULT(result);
			writeSamples += result.value() / 2;
		};
		return writeSamples;
	}
	else if (nBytes == 3)
	{
		int len1 = this->bufferLen / 3;
		wlen = len1;

		for (start = 0; start < nsamples; start += len1)
		{
			if (start + wlen >= nsamples)
			{
				wlen = nsamples - start;
			}
			SampleConv::FloattoInt24Byte(buffer + start, wlen, this->convBuffer);  //每次转换这么长
			auto result = this->WaveWriter::write(this->convBuffer, wlen * 3);  //转换过的数据写入
			CHECK_RESULT(result);
			writeSamples += result.value() / 3;
		};
		return writeSamples;
	}
	else if (nBytes == 4)
	{

		int len1 = this->bufferLen / 4;  //缓冲区有效长度
		int* i32ptr = (int*)(this->convBuffer);  //转换缓冲区
		wlen = len1;

		for (start = 0; start < nsamples; start += len1)
		{
			if (start + wlen >= nsamples)
			{
				wlen = nsamples - start;
			}
			SampleConv::FloattoInt32(buffer + start, wlen, i32ptr);  //每次转换这么长
			auto result = this->WaveWriter::write(this->convBuffer, wlen * 4);  //转换过的数据写入
			CHECK_RESULT(result);
			writeSamples += result.value() / 4;
		};
		return writeSamples;
	}
	else
	{
		return std::unexpected{ "暂不支持的格式" };
	}
}

std::expected<long, std::string> WaveWriter::writeSample(float value)
{
	int nBytes = this->_fmt->getBitsPerSample() / 8;
	long writeSamples = 0;
	if (this->_fmt->getEncoding() == WaveFormatEncoding::IeeeFloat)
	{
		char* ptr = reinterpret_cast<char*>(&value);
		auto result = this->write(ptr, 4);//浮点数不需要转换直接写入
		CHECK_RESULT(result);
		writeSamples = (result.value() / 4);
		return writeSamples;
	}

	if (nBytes == 2)
	{
		short* sptr = reinterpret_cast<short*>(this->convBuffer);  //转换缓冲区
		SampleConv::FloattoInt16(&value, 1,  sptr);  //每次转换这么长
		auto result = this->write(this->convBuffer, 2);  //转换过的数据写入
		CHECK_RESULT(result);
		writeSamples += result.value() / 2;
		return writeSamples;
	}
	else if (nBytes == 3)
	{

		SampleConv::FloattoInt24Byte(&value, 1, this->convBuffer);  //每次转换这么长
		auto result = this->write(this->convBuffer, 3);  //转换过的数据写入
		CHECK_RESULT(result);
		writeSamples += result.value() / 3;
		return writeSamples;
	}
	else if (nBytes == 4)
	{

		int* i32ptr = (int*)(this->convBuffer);  //转换缓冲区
		SampleConv::FloattoInt32(&value, 1, i32ptr);  //每次转换这么长
		auto result = this->write(this->convBuffer, 4);  //转换过的数据写入
		CHECK_RESULT(result);
		writeSamples += result.value() / 4;
		return writeSamples;
	}
	else
	{
		return std::unexpected{ "暂不支持的格式" };
	}
}



std::expected<long, std::string> WaveWriter::getPosition()
{
	auto pos = this->_stream->getPosition();
	if (!pos)
	{
		return pos;
	}

	return { pos.value() - this->_dataPos - 8};
}

std::expected<void, std::string> WaveWriter::setPosition(long value)
{
	auto len = this->_stream->getLength();
	
	if (value > len.value())
	{
		value = len.value();
	}
	value -= (value % this->_fmt->getBlockAlign());  //对齐采样块

	return this->_stream->setPosition(value + this->_dataPos + 8);
}

std::expected<long, std::string> WaveWriter::seek(long offset, SeekOrigin origin)
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

std::expected<long, std::string> WaveWriter::seekTime(long mills, SeekOrigin origin)
{
	auto bytes = this->_fmt->mills2Bytes(mills);
	return this->seek(bytes, origin);
}

std::expected<void, std::string> WaveWriter::setTimePos(long mills)
{
	long bytes = this->_fmt->mills2Bytes(mills);
	return this->setPosition(bytes);
}

std::expected<long, std::string> WaveWriter::getTimePos()
{
	auto pos = this->getPosition();
	if (!pos)
	{
		return pos;
	}
	return this->_fmt->bytes2Mills(pos.value());
}

std::expected<void, std::string> WaveWriter::writeWaveHeader()
{


	auto result = this->_stream->write("RIFF"); CHECK_RESULT(result);
	result = this->_stream->write("\0\0\0\0", 4); CHECK_RESULT(result); //riff块大小，占位，后续更新 4字节
	result = this->_stream->write("WAVE"); CHECK_RESULT(result);
	result = this->_stream->write("fmt "); CHECK_RESULT(result);
	auto ret = this->_fmt->writeTo(this->_stream); CHECK_RESULT(ret);

	result = this->_stream->getPosition();  CHECK_RESULT(result);//记录data块size的位置，后续更新
	this->_dataPos = result.value();

	result = this->_stream->write("data"); CHECK_RESULT(result);
	result = this->_stream->write("\0\0\0\0", 4);  CHECK_RESULT(result);//data块大小，占位，后续更新  4字节


	return {};
}


std::expected<void, std::string> WaveWriter::flush()
{
	auto result = this->updateHeader();   CHECK_RESULT(result); //更新文件头
	return this->_stream->flush();  //flush
}

long WaveWriter::getLength()
{
	return this->_dataSize;
}

long WaveWriter::getTotalMills()
{
	return this->_dataSize * 1000 / this->_fmt->getBytesPerSec();
}

const WaveFormat& WaveWriter::getWaveFormat()
{
	return *(this->_fmt);
}

std::expected<void, std::string> WaveWriter::updateHeader()
{
	BinaryStream bs(this->_stream);
	//获取当前写指针
	auto position = this->getPosition(); CHECK_RESULT(position);
	//更新RIFF
	auto result = this->_stream->seek(4, SeekOrigin::Begin); CHECK_RESULT(result);

	result = bs.write(int32_t(this->_stream->getLength().value() - 8)); CHECK_RESULT(result);

	//更新data块
	result = this->_stream->seek(this->_dataPos + 4, SeekOrigin::Begin); CHECK_RESULT(result);
	result = bs.write((int32_t)this->_dataSize); CHECK_RESULT(result);
	//恢复原来的位置
	auto ret = this->setPosition(position.value()); CHECK_RESULT(ret);
	return {};
}
