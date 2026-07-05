#pragma once
#include"Stream.h"
#include"WaveFormat.h"
#include<memory>
#include<vector>
#include"SampleConv.h"

struct RIFFChunk
{
	int identifier;
	int size;
	int offset;
};


class WaveReader
{

public:
	 
	WaveReader(Stream* stream);

	WaveReader(const std::string& filePath);

	virtual ~WaveReader();


	std::vector<RIFFChunk>& getExtraChunks();

	std::vector<char> getChunkData(RIFFChunk chunk);


public:

	std::expected<long, std::string> getPosition();
	//设置流位置
	std::expected<void, std::string> setPosition(long value);

	//设置偏移
	std::expected<long, std::string>  seek(long offset, SeekOrigin origin);
	//按照时间偏移
	std::expected<long, std::string> seekTime(long mills, SeekOrigin origin);

	std::expected<void, std::string> setTimePos(long mills);

	std::expected<long, std::string> getTimePos();


public:
	virtual std::expected<long, std::string> readSamples(float* buffer, int nsamples);
	
	virtual std::expected<long, std::string>  read(char* buffer, int size);

	virtual std::expected<long, std::string>  read(char* buffer, int size, int offset, int count);


	const WaveFormat& getWaveFormat() const;


	virtual long getLength();

	virtual long getFrameCount();

	virtual long getTotalMills();

private:

	std::expected<void, std::string> readWaveHeader();


private:

	std::unique_ptr<Stream> _ptr;

	Stream* _stream;

	long _dataPos{ 0 };

	long _dataSize{ 0 };   //data 块的大小，每次写入后更新

	std::vector<RIFFChunk> _extraChunks;

	std::unique_ptr<WaveFormat> _fmt;

	//转换缓冲区长度，单位字节
	const int bufferLen = 4096;

	char convBuffer[4096];
};





