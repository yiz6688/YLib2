#include"waveFormat.h"
#include<stdexcept>

//检查返回值如果失败返回错误
#define CHECK_RESULT(result) if (!result) { return std::unexpected{ result.error() }; }


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

WaveFormat::WaveFormat(Stream& reader)
{
    BinaryStream br(&reader);
    int formatChunkLength = br.readInt32().value();
    auto result = this->readFormat(&reader, formatChunkLength);
    if (!result)
    {
        throw std::runtime_error(result.error());
    }
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

    return std::format("fmtTag:{} sampleRate:{} channels:{} bitsPerSample:{} blockAlign:{}",
        fmtTag, this->sampleRate, this->channels, this->bitsPerSample, this->blockAlign);


}

bool WaveFormat::operator==(const WaveFormat& fmt)
{
    return this->waveFormatTag == fmt.waveFormatTag &&
        this->channels == fmt.channels &&
        this->sampleRate == fmt.sampleRate &&
        this->blockAlign == fmt.blockAlign &&
        this->bitsPerSample == fmt.bitsPerSample;
}

bool WaveFormat::operator!=(const WaveFormat& fmt)
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
    wfx.cbSize = this->extraSize;
    return wfx;
    
}

//从stream中读取waveformat, 输入读取的字节数量
std::expected<void, std::string> WaveFormat::readFormat(Stream* stream, int chunkSize)
{
    if (chunkSize < 16)
    {
        return std::unexpected("Invalid WaveFormat Structure");
    }
    BinaryStream br(stream);
    try
    {  
        //waveFormatTag = br.tryRead<std::uint16_t>();
        channels = br.tryRead<std::int16_t>();
        sampleRate = br.tryRead<std::int32_t>();
        bytesPerSec = br.tryRead<std::int32_t>();
        blockAlign = br.tryRead<std::int16_t>();
        bitsPerSample = br.tryRead<std::int16_t>();
        if (chunkSize > 16)
        {
            //扩展的情况下这里会是两个字节的代表扩展长度
            extraSize = br.tryRead<std::int16_t>();
            if (extraSize != chunkSize - 18)
            {
                extraSize = (short)(chunkSize - 18);
            }
        }
        else
        {
            extraSize = -1; //表示没有扩展块,跟扩展块是0进行区分
        }
    }catch(const std::exception& ex)
    {
        return std::unexpected{ ex.what() };
    }


    return {};
}

std::expected<void, std::string> WaveFormat::writeTo(Stream* stream)
{
    BinaryStream writer(stream);
    try
    {
        if (this->extraSize == -1)
        {
            writer.tryWrite(int32_t(16));
        }
        else
        {
            writer.tryWrite(int32_t(18 + this->extraSize));
        }
        writer.tryWrite(int16_t(this->waveFormatTag));
        writer.tryWrite(int16_t(this->channels));
        writer.tryWrite(int32_t(this->sampleRate));
        writer.tryWrite(int32_t(this->bytesPerSec));
        writer.tryWrite(int16_t(this->blockAlign));
        writer.tryWrite(int16_t(this->bitsPerSample));
        if (this->extraSize != -1)
        {
            writer.tryWrite(int16_t(this->extraSize));
        }
    }
    catch (const std::exception& ex)
    {
        return std::unexpected{ ex.what() };
    }
	return {};
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
	std::unique_ptr<WaveFormat> fmt;
    BinaryStream bs(&br);
	auto result = bs.readUInt16(); //CHECK_RESULT(result);
	auto fmtTag = result.value();
    if(fmtTag == WaveFormatEncoding::Extensible)
    {
		fmt = std::make_unique<WaveFormatExtensible>();
    }
    else if (formatChunkLength > 16)
    {
		fmt = std::make_unique<WaveFormatExtraData>();
    }
    else
    {
		fmt = std::make_unique<WaveFormat>();
	}
	auto ret = fmt->readFormat(&br, formatChunkLength);   //CHECK_RESULT(ret);
    return fmt;
}

WaveFormatExtraData::WaveFormatExtraData(Stream& stream)
    :WaveFormat(stream)
{
    if (this->extraSize > 0)
    {
        extraData.resize(this->extraSize);
        auto result = stream.read(extraData.data(), this->extraSize);
        if (!result)
        {
            throw std::runtime_error("Failed to read extra data");
        }
    }
}

std::unique_ptr<WaveFormat> WaveFormatExtraData::clone() const
{
    return std::make_unique<WaveFormatExtraData>(*this);
}

std::expected<void, std::string> WaveFormatExtraData::readFormat(Stream* stream, int chunkSize)
{
    auto result = this->WaveFormat::readFormat(stream, chunkSize); CHECK_RESULT(result);
    if (this->extraSize > 0)
    {
        this->extraData.resize(this->extraSize);
        auto ret = stream->read(this->extraData.data(), this->extraSize);
        CHECK_RESULT(ret);
    }
    return {};
}

std::expected<void, std::string> WaveFormatExtraData::writeTo(Stream* stream)
{
    auto result = this->WaveFormat::writeTo(stream);
    if (!result)
    {
        return result;
    }

    if (this->extraSize > 0)
    {
        auto ret = stream->write(this->extraData.data(), static_cast<int>(this->extraData.size()));
        if (!ret)
        {
            return std::unexpected{ "Failed to write extra data" };
        }
    }

    return {};
}

std::vector<char>& WaveFormatExtraData::getextraData()
{
    return this->extraData;
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

std::expected<void, std::string> WaveFormatExtensible::readFormat(Stream* stream, int chunkSize)
{
    auto result = this->WaveFormat::readFormat(stream, chunkSize); CHECK_RESULT(result);
    BinaryStream bs(stream);

    try
    {
        this->wValidBitsPerSample = bs.tryRead<short>();
        this->dwChannelMask = bs.tryRead<int>();
        this->subFormat.Data1 = bs.tryRead<int>();
        this->subFormat.Data2 = bs.tryRead<short>();
        this->subFormat.Data3 = bs.tryRead<short>();
    }
    catch (const std::exception& ex)
    {
        return std::unexpected{ ex.what() };
    }

    auto ret = stream->read(reinterpret_cast<char*>(this->subFormat.Data4), 8); CHECK_RESULT(ret);

    return {};
}

std::expected<void, std::string> WaveFormatExtensible::writeTo(Stream* stream)
{
    auto ret = this->WaveFormat::writeTo(stream); CHECK_RESULT(ret);
    
    BinaryStream bs(stream);
    auto result = bs.write(this->wValidBitsPerSample); CHECK_RESULT(result);
    result = bs.write(this->dwChannelMask); CHECK_RESULT(result);
    result = bs.write(this->subFormat.Data1); CHECK_RESULT(result);
    result = bs.write(this->subFormat.Data2); CHECK_RESULT(result);
    result = bs.write(this->subFormat.Data3); CHECK_RESULT(result);

    result = stream->write(reinterpret_cast<char*>(this->subFormat.Data4), 8); CHECK_RESULT(result);
    return {};
}
