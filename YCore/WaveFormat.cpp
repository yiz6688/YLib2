#include"base_config.hpp"
#include"waveFormat.h"
#include<stdexcept>
#include<cstring>
#include"ByteBuffer.h"

//仅在当前翻译单元定义 GUID 实体(供下方 DEFINE_GUID 生成定义,避免污染其它 TU)
#include<initguid.h>


//具体实现

WaveFormat::WaveFormat()
    : WaveFormat(48000, 16, 1)
{

}

WaveFormat::WaveFormat(int sampleRate, int channels)
    : WaveFormat(sampleRate, 16, channels)
{

}


WaveFormat::WaveFormat(int _sampleRate, int _bitdepth, int _channels)
    : waveFormatTag{ WaveFormatEncoding::Pcm }, 
    channels{ static_cast<short>(_channels) }, 
    sampleRate{ _sampleRate },
    bitsPerSample{ static_cast<short>(_bitdepth) },
    blockAlign{ static_cast<short>(_channels * _bitdepth / 8) },
    bytesPerSec{ sampleRate * blockAlign },
    extraSize{-1}
{
    if (channels < 1)
    {
        throw std::invalid_argument("channel不能小于1");
    }
}

//流应位于 fmt chunk 的 chunkSize 字段处
WaveFormat::WaveFormat(Stream& reader)
{
    BinaryStream br(&reader);
    int formatChunkLength = br.readInt32();
    //读取 format 标签(readFormat 默认从 channels 开始,标签须由调用者消费)
    auto tag = br.readUInt16();
    this->waveFormatTag = tag;
    this->readFormat(&reader, formatChunkLength);
}

int WaveFormat::mills2Bytes(int mills)
{
    int bytes = (int)((this->bytesPerSec / 1000.0) * mills);
    if ((bytes % this->blockAlign) != 0)  
    {
        // 数据块向上对齐
        bytes = bytes + this->blockAlign - (bytes % this->blockAlign);
    }
    return bytes;
}

long WaveFormat::bytes2Mills(int value)
{
    value -= (value % this->blockAlign);  //对齐采样块

    long mills = (long)((value / (this->bytesPerSec / 1000.0)));

    return mills;
}


std::string WaveFormat::toString() const
{
    std::string fmtTag = "";
    switch (waveFormatTag)
    {
    case WaveFormatEncoding::Pcm:
        fmtTag = "PCM"; break;
    case WaveFormatEncoding::IeeeFloat:
        fmtTag = "IEEE Float"; break;
    case WaveFormatEncoding::Extensible:
        fmtTag = "Extensible"; break;
    default:
        return "unknown fmt";
    }

    return fmt_ns::format("fmtTag:{} sampleRate:{} channels:{} bitsPerSample:{} blockAlign:{}",
        fmtTag, this->sampleRate, this->channels, this->bitsPerSample, this->blockAlign);


}

bool WaveFormat::operator==(const WaveFormat& fmt) const
{
    return this->waveFormatTag == fmt.waveFormatTag &&
        this->channels == fmt.channels &&
        this->sampleRate == fmt.sampleRate &&
        this->blockAlign == fmt.blockAlign &&
        this->bitsPerSample == fmt.bitsPerSample;
}

bool WaveFormat::operator!=(const WaveFormat& fmt) const
{
    return !(*this == fmt);
}

WAVEFORMATEX WaveFormat::toWaveFormatEx() const
{
    WAVEFORMATEX wfx;
    wfx.wFormatTag = this->waveFormatTag;
    wfx.nChannels = this->channels;
    wfx.nSamplesPerSec = this->sampleRate;
    wfx.nAvgBytesPerSec = this->bytesPerSec;
    wfx.nBlockAlign = this->blockAlign;
    wfx.wBitsPerSample = this->bitsPerSample;
    if(this->extraSize > 0)
    {
        wfx.cbSize = this->extraSize;
    }else
    {
        wfx.cbSize = 0;
    }

    return wfx;
    
}

//从stream中读取waveformat, 输入读取的字节数量
//流应位于 format 内容的 waveFormatTag 字段处(chunkSize 之后),tag 由调用者消费
//一次性读入整个 fmt 内容进 ByteBuffer, 再用内置 API 按类型解析(缓冲区模式, 数值转换 100% 成功)
void WaveFormat::readFormat(Stream* stream, int chunkSize)
{
    if (chunkSize < 16)
    {
        throw std::runtime_error("Invalid WaveFormat Structure");
    }
    if (chunkSize > (1 << 20))
    {
        throw std::runtime_error("fmt chunk 过大");
    }

    //tag(2字节)已由调用者读取,剩余内容长度为 chunkSize - 2
    int bodySize = chunkSize - 2;
    ByteBuffer bb = ByteBuffer::allocate(static_cast<size_t>(bodySize));
    long n = stream->read(bb.data(), bodySize);
    if (n < bodySize)
    {
        throw std::runtime_error("fmt 数据长度不足");
    }

    channels = static_cast<short>(bb.readUInt16());
    sampleRate = bb.readInt32();
    bytesPerSec = bb.readInt32();
    blockAlign = static_cast<short>(bb.readUInt16());
    bitsPerSample = static_cast<short>(bb.readUInt16());
    if (chunkSize > 16)
    {
        extraSize = static_cast<short>(bb.readUInt16());
        if (extraSize != chunkSize - 18)
        {
            extraSize = static_cast<short>(chunkSize - 18);
        }
        //扩展数据(原 WaveFormatExtraData 的功能已并入基类)
        size_t left = bb.remaining();
        if (left > 0)
        {
            auto sp = bb.readableSpan(left);
            this->extraData.assign(sp.begin(), sp.end());
        }
    }
    else
    {
        extraSize = -1; //表示没有扩展块,跟扩展块是0进行区分
    }
}

void WaveFormat::writeTo(Stream* stream)
{
    //一次性构建整个 fmt 头(含 4 字节 chunkSize 字段)再写入, 避免多次小写入
    ByteBuffer bb = ByteBuffer::allocate(32 + this->extraData.size());

    if (this->extraSize == -1)
    {
        bb.writeUInt32(16);
    }
    else
    {
        bb.writeUInt32(18 + this->extraSize);
    }
    bb.writeUInt16(static_cast<std::uint16_t>(this->waveFormatTag));
    bb.writeUInt16(static_cast<std::uint16_t>(this->channels));
    bb.writeUInt32(static_cast<std::uint32_t>(this->sampleRate));
    bb.writeUInt32(static_cast<std::uint32_t>(this->bytesPerSec));
    bb.writeUInt16(static_cast<std::uint16_t>(this->blockAlign));
    bb.writeUInt16(static_cast<std::uint16_t>(this->bitsPerSample));
    if (this->extraSize != -1)
    {
        bb.writeUInt16(static_cast<std::uint16_t>(this->extraSize));
        //扩展数据:非 Extensible 格式的原始 extra 数据写在这里;
        //Extensible 的扩展是结构化字段,由子类 writeTo 写入,其 extraData 保持为空
        if (!this->extraData.empty())
        {
            bb.writeBytes(this->extraData.data(), this->extraData.size());
        }
    }

    stream->write(bb.data(), static_cast<int>(bb.position()));
}

std::unique_ptr<WaveFormat> WaveFormat::clone() const
{
    return std::make_unique<WaveFormat>(*this);
}



WaveFormat WaveFormat::createCustomFormat(short tag, int sampleRate, int bitDepth, int channels)
{
    WaveFormat fmt(sampleRate, bitDepth, channels);
    fmt.waveFormatTag = tag;
    fmt.extraSize = -1;
    return fmt;
}

WaveFormat WaveFormat::createFloatWaveFormat(int sampleRate, int channels)
{
    WaveFormat fmt(sampleRate, 32, channels);
    fmt.waveFormatTag = WaveFormatEncoding::IeeeFloat;
    return fmt;
}


std::unique_ptr<WaveFormat> WaveFormat::fromFormatChunk(Stream& br, int formatChunkLength)
{
	//流位于 fmt chunk 内容起始(chunkSize 之后), 先读取 format tag 用于分派
    BinaryStream bs(&br);
	auto fmtTag = bs.readUInt16();
    std::unique_ptr<WaveFormat> fmt;
    if(fmtTag == WaveFormatEncoding::Extensible)
    {
		fmt = std::make_unique<WaveFormatExtensible>();
    }
    else
    {
		//非 Extensible:即使 chunkSize>16 带有扩展数据,也由基类直接承载(原 WaveFormatExtraData 已合并)
		fmt = std::make_unique<WaveFormat>();
	}
	//读取到的 tag 必须写回格式对象(原代码遗漏,导致解析后 tag 恒为默认的 PCM)
	fmt->waveFormatTag = fmtTag;
	fmt->readFormat(&br, formatChunkLength);  //失败抛出异常
    return fmt;
}


#ifdef __MINGW32__
    // KSDATAFORMAT_SUBTYPE_PCM
    DEFINE_GUID(KSDATAFORMAT_SUBTYPE_PCM,
        0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

    // KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
    DEFINE_GUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT,
        0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
#endif // __MINGW32__




WaveFormatExtensible::WaveFormatExtensible(int rate, int bits, int channels)
    :WaveFormat(rate, bits, channels)
{
    this->waveFormatTag = WaveFormatEncoding::Extensible;
    extraSize = 22;
    this->dwChannelMask = 0;
    this->wValidBitsPerSample = (short)bits;
    for (int n = 0; n < channels; n++)
    {
        dwChannelMask |= (1 << n);
    }
    if (bits == 32)
    {
        // KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
        subFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    }
    else
    {
        // KSDATAFORMAT_SUBTYPE_PCM
        subFormat = KSDATAFORMAT_SUBTYPE_PCM;
    }
}

WaveFormatExtensible::WaveFormatExtensible(WaveFormat fmt)
    :WaveFormat(fmt)
{
    this->waveFormatTag = WaveFormatEncoding::Extensible;
    this->extraSize = 22;
    this->wValidBitsPerSample = fmt.getBitsPerSample();

    this->dwChannelMask = 0;
    for (int n = 0; n < fmt.getChannels(); n++)
    {
        dwChannelMask |= (1 << n);
    }

    if (fmt.getEncoding() == WaveFormatEncoding::IeeeFloat)
    {
        subFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    }
    else
    {
        subFormat = KSDATAFORMAT_SUBTYPE_PCM;
    }
}

//WaveFormat WaveFormatExtensible::toWaveFormat()
//{
//    return WaveFormat(this->sampleRate, this->bitsPerSample, this->channels);
//}

WaveFormatExtensible::operator WAVEFORMATEXTENSIBLE()
{
    WAVEFORMATEXTENSIBLE fmtExt;

    fmtExt.Format.wFormatTag = this->waveFormatTag;
    fmtExt.Format.nChannels = this->channels;
    fmtExt.Format.nSamplesPerSec = this->sampleRate;
    fmtExt.Format.nAvgBytesPerSec = this->bytesPerSec;
    fmtExt.Format.nBlockAlign = this->blockAlign;
    fmtExt.Format.wBitsPerSample = this->bitsPerSample;
    fmtExt.Format.cbSize = this->extraSize;

    fmtExt.dwChannelMask = this->dwChannelMask;
    fmtExt.Samples.wValidBitsPerSample = this->wValidBitsPerSample;
    fmtExt.SubFormat = this->subFormat;

    return fmtExt;
}

std::unique_ptr<WaveFormat> WaveFormatExtensible::clone() const
{
    return std::make_unique<WaveFormatExtensible>(*this);
}

void WaveFormatExtensible::readFormat(Stream* stream, int chunkSize)
{
    //基类读取 tag 之后的字段 + extraSize + 22 字节扩展原始数据(存入 extraData)
    this->WaveFormat::readFormat(stream, chunkSize);

    //从 extraData 中解析 WAVEFORMATEXTENSIBLE 结构化字段,避免重复读取流
    if (this->extraData.size() < 22)
    {
        throw std::runtime_error("Extensible 扩展数据不足 22 字节");
    }
    const char* p = this->extraData.data();
    std::memcpy(&this->wValidBitsPerSample, p, 2);
    std::memcpy(&this->dwChannelMask, p + 2, 4);
    std::memcpy(&this->subFormat.Data1, p + 6, 4);
    std::memcpy(&this->subFormat.Data2, p + 10, 2);
    std::memcpy(&this->subFormat.Data3, p + 12, 2);
    std::memcpy(this->subFormat.Data4, p + 14, 8);
    //解析完成后清空,writeTo 由子类写入结构化字段,避免重复输出
    this->extraData.clear();
}

void WaveFormatExtensible::writeTo(Stream* stream)
{
    this->WaveFormat::writeTo(stream);
    
    BinaryStream bs(stream);
    bs.write(this->wValidBitsPerSample);
    bs.write(this->dwChannelMask);
    bs.write(this->subFormat.Data1);
    bs.write(this->subFormat.Data2);
    bs.write(this->subFormat.Data3);

    stream->write(reinterpret_cast<char*>(this->subFormat.Data4), 8);
}
