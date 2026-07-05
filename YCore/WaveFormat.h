#pragma once
#include <stdexcept>
#include<string>
#include<vector>
#include<format>
#include<memory>

#include<initguid.h>
#include<Windows.h>
#include <mmreg.h>
#include<expected>

#include"BinaryStream.h"



struct WaveFormatEncoding
{
    /// 未知的类型
    static constexpr short  Unknown = 0x0000;

    /// PCM 整数格式
    static constexpr short Pcm = 0x0001;

    /// PCM 浮点数格式
    static constexpr short IeeeFloat = 0x0003;

    /// 波形文件扩展格式，微软定义
    static constexpr unsigned short  Extensible = 0xFFFE;   
};


enum class SampleType : int
{
	UNKNOWN = 0, //未知类型

    IEEE32 = 1,   //32位浮点数
    INT16  = 2, //16位整数
    INT24  = 3, //24位整数
    INT32  = 4, //32位整数

    INT64,     //64位整数
    IEEE64    //64位浮点数
};

struct Chunk
{

private:
    int identifier;
    int length;

};


struct WaveFormat
{
 
public:
    WaveFormat();
    WaveFormat(int sampleRate, int channels);
    WaveFormat(int rate, int bits, int channels);

    WaveFormat(const WaveFormat&) = default;
    WaveFormat(WaveFormat&&) = default;

    WaveFormat& operator=(const WaveFormat&) = default;
    WaveFormat& operator=(WaveFormat&&) = default;

    //从流中读取
    WaveFormat(Stream& reader);
  
	~WaveFormat() = default;

    int mills2Bytes(int mills);

    long bytes2Mills(int value);
   
    std::string toString() const;
        
    bool operator == (const WaveFormat& fmt);

    bool operator !=(const WaveFormat& fmt);
 
	WAVEFORMATEX toWaveFormatEx() const;

    //纯waveformat的位置读取
    virtual std::expected<void, std::string> readFormat(Stream* stream, int chunkSize);

    virtual std::expected<void, std::string> writeTo(Stream* stream);

    virtual std::unique_ptr<WaveFormat> clone() const;


    static WaveFormat createFloatWaveFormat(int sampleRate, int channels);

    static WaveFormat createCustomFormat(short fmt, int sampleRate, int channels, int bitDepth);

    //从流中读取format对象，多态返回
    static std::unique_ptr<WaveFormat> fromFormatChunk(Stream& br, int formatChunkLength);


public:

    int getEncoding() const { return this->waveFormatTag; }

    int getChannels() const { return this->channels; }

    int getSampleRate() const { return this->sampleRate; }

    int getBytesPerSec() const { return this->bytesPerSec; }

    int getBlockAlign() const { return this->blockAlign; }

    int getBitsPerSample() const { return this->bitsPerSample; }

    int getExtraSize() const { return this->extraSize; }

protected:
    // 音频格式类型
    unsigned short waveFormatTag;
    // 通道数
    short channels;
    // 采样率
    int sampleRate;
    // 每通道位数
    short bitsPerSample;
    // 块大小
    short blockAlign;
    // 每秒平均字节数
    int bytesPerSec;
    // 扩展大小
    short extraSize{ -1 };
};



/*
    读取扩展数据的类，
*/
class WaveFormatExtraData : public WaveFormat
{

public:
    WaveFormatExtraData(const WaveFormatExtraData&) = default;
    WaveFormatExtraData(WaveFormatExtraData&&) = default;

    WaveFormatExtraData& operator=(const WaveFormatExtraData&) = default;
    WaveFormatExtraData& operator=(WaveFormatExtraData&&) = default;

    WaveFormatExtraData() = default;
    WaveFormatExtraData(Stream& stream);
    ~WaveFormatExtraData() = default;

public:

    std::unique_ptr<WaveFormat> clone() const override;

    std::expected<void, std::string> readFormat(Stream* stream, int chunkSize) override;

    std::expected<void, std::string> writeTo(Stream* stream) override;

    std::vector<char>& getextraData();
private:
    std::vector<char> extraData;
};




/*
    扩展格式formatTag = FFFE
*/
class WaveFormatExtensible : public WaveFormat
{

public:
    WaveFormatExtensible() = default;
    WaveFormatExtensible(const WaveFormatExtensible&) = default;
    WaveFormatExtensible(WaveFormatExtensible&&) = default;

    WaveFormatExtensible& operator=(const WaveFormatExtensible&) = default;
    WaveFormatExtensible& operator=(WaveFormatExtensible&&) = default;

    WaveFormatExtensible(int rate, int bits, int channels);
    WaveFormatExtensible(WaveFormat fmt);


    //转换为标准wav头
    //WaveFormat toWaveFormat();

    //操作符重载
    operator WAVEFORMATEXTENSIBLE ();

public:
    std::unique_ptr<WaveFormat> clone() const override;

    std::expected<void, std::string> readFormat(Stream* stream, int chunkSize) override;

    std::expected<void, std::string> writeTo(Stream* stream) override;


private:
    short wValidBitsPerSample;  // bits of precision, or is wSamplesPerBlock if wBitsPerSample==0
    int dwChannelMask;          // 通道掩码
    GUID subFormat;

};




