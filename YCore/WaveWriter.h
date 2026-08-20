#pragma once
#include"Stream.h"
#include"WaveFormat.h"
#include"SampleConv.h"
#include<memory>
#include"TResult.h"

class WaveWriter
{


private:
	WaveWriter(const WaveFormat& _waveFormat, Stream* stream);

	WaveWriter(const WaveFormat& _waveFormat, TPtr<Stream>&& stream);

public:	
	virtual ~WaveWriter();

public:
	//static TPResult<WaveWriter> create(const WaveFormat& fmt, Stream* stream);
	
	//static TPResult<WaveWriter> create(const WaveFormat& fmt, std::string_view filepath);


	virtual std::expected<long, std::string> write(char* buffer, int size, int offset, int count);

	virtual std::expected<long, std::string> write(char* buffer, int count);

	template<typename T>
	std::expected<long, std::string> write(T* buffer, int nsamples)
	{
		if (sizeof(T) * 8 != this->_fmt->getBitsPerSample())
		{
			return std::unexpected{ "写入块没有对齐" };
		}

		char* ptr = reinterpret_cast<char*>(buffer);
		return this->write(ptr, nsamples * sizeof(T)); 
	}

	//写浮点数
	std::expected<long, std::string> writeSamples(float* buffer, int nsamples);
	
	std::expected<long, std::string> writeSample(float value);

	std::expected<long, std::string> getPosition();
	//设置流位置
	std::expected<void, std::string> setPosition(long value);
	
	//设置偏移
	std::expected<long, std::string>  seek(long offset, SeekOrigin origin);
	//按照时间偏移
	std::expected<long, std::string> seekTime(long mills, SeekOrigin origin);

	std::expected<void, std::string> setTimePos(long mills);
	
	std::expected<long, std::string> getTimePos();

	virtual std::expected<void, std::string> flush();


	long getLength();

	long getTotalMills();

	const WaveFormat& getWaveFormat();

private:
	std::expected<void, std::string> writeWaveHeader();


protected:
	

	virtual std::expected<void, std::string> updateHeader();

public:
	static TPResult<WaveWriter> create(const WaveFormat& _waveFormat, std::string_view filepath);

	static TPResult<WaveWriter> create(const WaveFormat& _waveFormat, Stream* stream);

protected:
	
	std::unique_ptr<Stream> _ptr;

	Stream* _stream;

	long _dataPos{ 0 };

	long _dataSize{ 0 };

	//转换缓冲区长度，单位字节
	const int bufferLen = 4096;

	char convBuffer[4096];

	std::unique_ptr<WaveFormat> _fmt;


};