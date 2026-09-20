#pragma once
#include"base_config.hpp"
#include"WaveStream.h"
#include"WaveBuffer.h"
#include"TResult.h"
#include<memory>

//丰富写层(高层 API): 组合 WaveStream(所有权或使用权), 内置 WaveBuffer 转换桥,
//支持按浮点(float/double)或原生类型(Sample)精细化写入。
//资源所有权约定同 WaveStream: unique_ptr 持有所有权, 裸指针持有使用权。
//创建统一走静态工厂, 返回 TPResult(不抛异常):
//- create(format, filepath)                 : 一次性从文件创建(内部创建并持有 WaveStream);
//- create(WaveStream&)                      : 借用已存在的 WaveStream(使用权);
//- create(unique_ptr<WaveStream>&&)         : 转移 WaveStream 所有权。
class WaveWriter
{

private:
	//借用, 组合已有 WaveStream(使用权)
	WaveWriter(WaveStream& stream, SampleType storageType);

	//所有权, 持有 WaveStream(转移)
	WaveWriter(std::unique_ptr<WaveStream>&& stream, SampleType storageType);

public:
	//写入交织浮点(float/double), 返回写入的采样数(总采样, 含通道)
	int writeFloat(float* buffer, int sampleNum);
	int writeFloat(double* buffer, int sampleNum);

	//写入交织原始数据(原生类型, Sample 描述缓冲), 返回写入的帧数
	int writeRaw(Sample& sample, int sampleNum);

	//写入原始字节(透传到底层 WaveStream, data 区), 返回实际写入的字节数
	long write(char* buffer, int size, int offset, int count);
	long write(char* buffer, int size);

	//基础信息代理
	const WaveFormat& getWaveFormat() const;
	long getLength();
	long getFrameCount();
	long getTotalMills();
	int getChannels();

	//刷新(更新文件头并落盘), 失败抛出异常
	void flush();

	//位置/时间代理
	long getPosition();
	long seek(long offset, SeekOrigin origin);
	long getTimePos();

public:
	static TPResult<WaveWriter> create(const WaveFormat& waveFormat, std::string_view filepath);
	static TPResult<WaveWriter> create(WaveStream& stream);
	static TPResult<WaveWriter> create(std::unique_ptr<WaveStream>&& stream);

private:
	template<typename F>
	int writeFloatImpl(F* buffer, int sampleNum);

private:
	std::unique_ptr<WaveStream> _ptr;   //所有权

	WaveStream* _stream;                //使用权

	//内置 WaveBuffer(转换/缓冲桥)
	std::unique_ptr<WaveBuffer> _wb;

	//存储类型(由 stream 格式映射)
	SampleType _storageType;

	//环形区单块帧数
	int _chunkFrames{ 1024 };
};
