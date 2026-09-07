#pragma once
#include"Stream.h"
#include"WaveFormat.h"
#include"TResult.h"
#include<memory>
#include<vector>

struct RIFFChunk
{
	int identifier;
	int size;
	int offset;
};

//底层通用 WAV 流(duplex): 同时支持读与写, 组合底层 Stream(不继承)。
//资源所有权约定: unique_ptr 持有所有权, 裸指针持有使用权。
//- open  : 打开已有 wav, 解析头部, 可读(底层可写时可读写/追加);
//- create: 新建 wav, 写入头部, 用于写入。
//内部方法为异常模式(失败抛出 std::runtime_error), 仅在 open/create 工厂 try 转 expected。
//读/写/定位共享同一套 data 区偏移语义: getPosition/setPosition 均为 data 区相对字节偏移。
class WaveStream
{

private:
	//借用, 打开已有 wav(解析头)
	WaveStream(Stream* stream);

	//所有权, 打开已有 wav(解析头)
	WaveStream(std::unique_ptr<Stream>&& stream);

	//借用, 新建 wav(写头)
	WaveStream(const WaveFormat& waveFormat, Stream* stream);

	//所有权, 新建 wav(写头)
	WaveStream(const WaveFormat& waveFormat, std::unique_ptr<Stream>&& stream);

public:
	virtual ~WaveStream();

	//基础 I/O(data 区), 读写对齐采样块/限制在 data 区, 失败抛出异常
	long read(char* buffer, int size, int offset, int count);
	long read(char* buffer, int size);

	long write(char* buffer, int size, int offset, int count);
	long write(char* buffer, int size);

	//位置/时间(均为 data 区相对语义), 失败抛出异常
	long getPosition();
	void setPosition(long value);
	long seek(long offset, SeekOrigin origin);
	long seekTime(long mills, SeekOrigin origin);
	void setTimePos(long mills);
	long getTimePos();

	const WaveFormat& getWaveFormat() const;
	long getLength();          //data 区字节数
	long getFrameCount();
	long getTotalMills();
	int getChannels();

	//读专属(打开已有文件时解析)
	std::vector<RIFFChunk>& getExtraChunks();
	std::vector<char> getChunkData(RIFFChunk chunk);

	//写专属: 更新文件头(RIFF/data 大小)并落盘, 失败抛出异常
	void flush();
	void updateHeader();

	//释放底层流
	void close();

	//打开已有 wav(读/读写), 工厂: 失败转 expected
	static TPResult<WaveStream> open(std::string_view filepath);
	static TPResult<WaveStream> open(Stream* stream);

	//新建 wav(写), 工厂: 失败转 expected
	static TPResult<WaveStream> create(const WaveFormat& waveFormat, std::string_view filepath);
	static TPResult<WaveStream> create(const WaveFormat& waveFormat, Stream* stream);

private:
	void readWaveHeader();
	void writeWaveHeader();

private:
	std::unique_ptr<Stream> _ptr;   //所有权

	Stream* _stream;                //使用权

	long _dataPos{ 0 };             //data 块头的原始流偏移

	long _dataSize{ 0 };            //data 块大小, 写入后更新

	std::vector<RIFFChunk> _extraChunks;

	std::unique_ptr<WaveFormat> _fmt;
};
