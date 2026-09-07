#include"WaveStream.h"
#include"FileStream.h"
#include"BitConverter.h"
#include"BinaryStream.h"
#include<algorithm>
#include<cstring>
#include<stdexcept>

using namespace std;

const int rf64ChunkId = BitConverter::Converter<int>("RF64");
const int riffChunkId = BitConverter::Converter<int>("RIFF");
const int waveChunkId = BitConverter::Converter<int>("WAVE");
const int dataChunkId = BitConverter::Converter<int>("data");
const int formatChunkId = BitConverter::Converter<int>("fmt ");

class WaveChunkReader
{
public:
	WaveChunkReader() {}
	~WaveChunkReader() {}

	void ReadWaveHeader(Stream* stream)
	{
		long position = stream->getPosition();  //当前所在位置

		long size = stream->getLength();        //流总长度
		long remainSize = size - position;      //剩余长度

		bool fmtchunkFlag = false;  //是否读取到format chunk的标志

		//一次性读取 12 字节 RIFF 头: "RIFF"(4) + 文件长度(4) + "WAVE"(4), 避免三次小读取
		char riffHeader[12];
		long rr = stream->read(riffHeader, 12);
		if (rr < 12)
		{
			throw std::runtime_error("Not a WAVE file - header too short");
		}
		auto load32 = [](const char* p) {
			return static_cast<long>(static_cast<unsigned char>(p[0])
				| (static_cast<unsigned char>(p[1]) << 8)
				| (static_cast<unsigned char>(p[2]) << 16)
				| (static_cast<unsigned char>(p[3]) << 24));
		};

		int riffID = static_cast<int>(load32(riffHeader));
		if (riffID != riffChunkId)
		{
			throw std::runtime_error("Not a WAVE file - no RIFF header");
		}
		this->_riffSize = load32(riffHeader + 4);

		int waveID = static_cast<int>(load32(riffHeader + 8));
		if (waveID != waveChunkId)
		{
			throw std::runtime_error("Not a WAVE file - no WAVE header");
		}

		//RF64 的 riffSize 为 0xFFFFFFFF(-1), 此时无法用它计算范围, 退化为按剩余长度
		long riffEnd = (this->_riffSize >= 0) ? (this->_riffSize + 8) : remainSize;
		long endpos = std::min<long>(riffEnd, remainSize);

		position = stream->getPosition();

		char chunkHeader[8];
		while (position <= endpos - 8)
		{
			//一次性读取 8 字节 chunk 头: 类型(4) + 大小(4)
			long rr2 = stream->read(chunkHeader, 8);
			if (rr2 < 8)
			{
				break;
			}
			long chunkHeaderPos = position;  //chunk 头起始的原始偏移
			int chunkID = static_cast<int>(load32(chunkHeader));
			long chunkSize = load32(chunkHeader + 4);

			if (chunkID == dataChunkId)
			{
				this->_dataPos = chunkHeaderPos;
				//data 内容长度: 若 chunkSize 为负(如 RF64 哨兵值 0xFFFFFFFF)或超过剩余长度, 取剩余
				long remainData = size - chunkHeaderPos - 8;
				this->_dataSize = (chunkSize >= 0 && chunkSize <= remainData) ? chunkSize : remainData;

				if (fmtchunkFlag == false)
				{
					throw std::runtime_error("Invalid WAV file - fmt chunk must be before data chunk");
				}

				auto nn = this->_fmt->getBlockAlign();
				this->_dataSize -= (this->_dataSize % nn);  //对齐采样块

				//移动指针到下一个chunk(负长度按 0 处理,避免 seek 后退)
				if (chunkSize > 0)
				{
					position = stream->seek(chunkSize, SeekOrigin::Current);
				}
				else
				{
					position = chunkHeaderPos + 8;
				}
			}
			else if (chunkID == formatChunkId)
			{
				if (chunkSize < 16 || chunkSize > (1 << 20))
				{
					throw std::runtime_error("Format chunk length is invalid");
				}
				//获取waveformat,这里不用移动了,下面的读取会自动移动指针
				this->_fmt = WaveFormat::fromFormatChunk(*stream, static_cast<int>(chunkSize));
				fmtchunkFlag = true;
				position = stream->getPosition();
				continue;
			}
			else
			{
				if (chunkSize < 0 || chunkSize > size - chunkHeaderPos - 8)
				{
					//如果chunkSize超过剩余长度或非法, 说明文件损坏了
					break;
				}

				this->_riffLst.emplace_back(RIFFChunk{ chunkID, static_cast<int>(chunkSize), chunkHeaderPos + 8 });
				//移动指针到下一个chunk
				position = stream->seek(chunkSize, SeekOrigin::Current);
			}
		}

		if (fmtchunkFlag == false)
		{
			throw std::runtime_error("Invalid WAV file - No fmt chunk found");
		}

		if (this->_dataPos == 0)
		{
			throw std::runtime_error("Invalid WAV file - No data chunk found");
		}
	}

public:
	std::unique_ptr<WaveFormat> _fmt;

	long _riffSize{ 0 };

	long _dataPos{ 0 };

	long _dataSize{ 0 };

	vector<RIFFChunk> _riffLst{};
};

WaveStream::WaveStream(Stream* stream)
	: _ptr{ nullptr }, _stream(stream), _dataPos{ 0 }, _dataSize{ 0 }
{
	this->readWaveHeader();
}

WaveStream::WaveStream(std::unique_ptr<Stream>&& stream)
	: _ptr{ std::move(stream) }, _stream(_ptr.get()), _dataPos{ 0 }, _dataSize{ 0 }
{
	this->readWaveHeader();
}

WaveStream::WaveStream(const WaveFormat& waveFormat, Stream* stream)
	: _ptr{ nullptr }, _stream(stream), _dataPos{ 0 }, _dataSize{ 0 }, _fmt{ waveFormat.clone() }
{
	this->writeWaveHeader();
}

WaveStream::WaveStream(const WaveFormat& waveFormat, std::unique_ptr<Stream>&& stream)
	: _ptr{ std::move(stream) }, _stream(_ptr.get()),
	_dataPos{ 0 }, _dataSize{ 0 }, _fmt{ waveFormat.clone() }
{
	this->writeWaveHeader();
}

WaveStream::~WaveStream()
{
	//析构时尽量刷新头部并落盘, 保证文件完整
	try
	{
		this->flush();
	}
	catch (...)
	{
		//析构不抛异常
	}
}

long WaveStream::read(char* buffer, int size, int offset, int count)
{
	if (offset < 0 || size < 0 || count < 0 || offset > size - count)
	{
		throw std::runtime_error("输入参数不合法");
	}
	if (count == 0)
	{
		return 0;
	}
	long pos = this->getPosition();
	long value = this->_dataSize - pos;
	if (value <= 0)
	{
		return 0;
	}
	if (value > count)
	{
		value = count;
	}
	return this->_stream->read(buffer + offset, static_cast<int>(value));
}

long WaveStream::read(char* buffer, int size)
{
	return this->read(buffer, size, 0, size);
}

long WaveStream::write(char* buffer, int size, int offset, int count)
{
	if (offset < 0 || size < 0 || count < 0 || offset > size - count)
	{
		throw std::runtime_error("输入参数不合法");
	}
	if (count == 0)
	{
		return 0;
	}
	if (count % this->_fmt->getBlockAlign() != 0)
	{
		throw std::runtime_error("写入块没有对齐");
	}
	long n = this->_stream->write(buffer, size, offset, count);
	this->_dataSize += n;  //更新data块大小
	//注意: 不每次写都 updateHeader(原会 O(n^2)), 只在 flush()/析构时更新一次
	return n;
}

long WaveStream::write(char* buffer, int size)
{
	return this->write(buffer, size, 0, size);
}

long WaveStream::getPosition()
{
	return this->_stream->getPosition() - this->_dataPos - 8;
}

void WaveStream::setPosition(long value)
{
	this->_stream->getLength();  //失败抛出异常

	if (value > this->_dataSize)
	{
		value = this->_dataSize;
	}
	if (value < 0)
	{
		value = 0;
	}
	value -= (value % this->_fmt->getBlockAlign());  //对齐采样块

	this->_stream->setPosition(value + this->_dataPos + 8);
}

long WaveStream::seek(long offset, SeekOrigin origin)
{
	long target = 0;
	if (origin == SeekOrigin::Begin)
	{
		target = offset;
	}
	else if (origin == SeekOrigin::Current)
	{
		target = this->getPosition() + offset;   //相对当前 data 内位置
	}
	else if (origin == SeekOrigin::End)
	{
		target = this->_dataSize + offset;  //相对 data 块末尾
	}
	else
	{
		throw std::runtime_error("不合法的seekOrigin");
	}

	this->setPosition(target);

	return this->getPosition();
}

long WaveStream::seekTime(long mills, SeekOrigin origin)
{
	auto bytes = this->_fmt->mills2Bytes(mills);
	return this->seek(bytes, origin);
}

void WaveStream::setTimePos(long mills)
{
	long bytes = this->_fmt->mills2Bytes(mills);
	this->setPosition(bytes);
}

long WaveStream::getTimePos()
{
	return this->_fmt->bytes2Mills(this->getPosition());
}

const WaveFormat& WaveStream::getWaveFormat() const
{
	return *(this->_fmt);
}

long WaveStream::getLength()
{
	return this->_dataSize;
}

long WaveStream::getFrameCount()
{
	return this->_dataSize / this->_fmt->getBlockAlign();
}

long WaveStream::getTotalMills()
{
	return this->_dataSize * 1000 / this->_fmt->getBytesPerSec();
}

int WaveStream::getChannels()
{
	return this->_fmt->getChannels();
}

std::vector<RIFFChunk>& WaveStream::getExtraChunks()
{
	return this->_extraChunks;
}

std::vector<char> WaveStream::getChunkData(RIFFChunk chunk)
{
	if (chunk.size < 0)
	{
		throw std::runtime_error("非法 chunk 大小");
	}
	std::vector<char> data(static_cast<size_t>(chunk.size));
	//chunk.offset 是原始流偏移, 直接定位
	this->_stream->setPosition(chunk.offset);
	long n = this->_stream->read(data.data(), static_cast<int>(data.size()));
	data.resize(static_cast<size_t>(n));
	return data;
}

void WaveStream::flush()
{
	this->updateHeader();  //更新文件头
	this->_stream->flush();
}

void WaveStream::updateHeader()
{
	//读取当前写指针(相对位置)
	long position = this->getPosition();

	long fileLen = this->_stream->getLength();

	//更新 RIFF 大小(偏移 4)
	{
		int32_t v = static_cast<int32_t>(fileLen - 8);
		char buf[4] = { static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
			static_cast<char>((v >> 16) & 0xFF), static_cast<char>((v >> 24) & 0xFF) };
		this->_stream->seek(4, SeekOrigin::Begin);
		this->_stream->write(buf, 4);
	}

	//更新 data 块大小(偏移 _dataPos + 4)
	{
		int32_t v = static_cast<int32_t>(this->_dataSize);
		char buf[4] = { static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
			static_cast<char>((v >> 16) & 0xFF), static_cast<char>((v >> 24) & 0xFF) };
		this->_stream->seek(this->_dataPos + 4, SeekOrigin::Begin);
		this->_stream->write(buf, 4);
	}

	//恢复原来的位置
	this->setPosition(position);
}

void WaveStream::close()
{
	this->_stream->close();
}

void WaveStream::readWaveHeader()
{
	WaveChunkReader chunkReader;
	chunkReader.ReadWaveHeader(this->_stream);

	this->_fmt = std::move(chunkReader._fmt);
	this->_dataPos = chunkReader._dataPos;
	this->_dataSize = chunkReader._dataSize;
	this->_extraChunks = std::move(chunkReader._riffLst);

	this->setPosition(0);  //设置到data区起始位置
}

void WaveStream::writeWaveHeader()
{
	//一次性写入 16 字节 RIFF/WAVE/fmt 头(修复原 copy_n 参数颠倒导致写全 0 的 bug)
	char buffer[16] = { 0 };
	std::copy_n("RIFF\0\0\0\0WAVEfmt ", 16, buffer);
	this->_stream->write(buffer, 16);

	this->_fmt->writeTo(this->_stream);

	this->_dataPos = this->_stream->getPosition();  //记录data块size的位置, 后续更新

	//一次性写入 "data" + 4 字节占位大小
	const char dataHeader[8] = { 'd','a','t','a', 0,0,0,0 };
	this->_stream->write(dataHeader, 8);
}

TPResult<WaveStream> WaveStream::open(std::string_view filepath)
{
	try
	{
		auto fs = FileStream::create(filepath, FileMode::Open, FileAccess::Read);
		TPtr<WaveStream> ptr = TPtr<WaveStream>(new WaveStream(std::move(fs)));
		return ptr;
	}
	catch (const std::exception& e)
	{
		return make_err<WaveStream>(e.what());
	}
}

TPResult<WaveStream> WaveStream::open(Stream* stream)
{
	try
	{
		TPtr<WaveStream> ptr = TPtr<WaveStream>(new WaveStream(stream));
		return ptr;
	}
	catch (const std::exception& e)
	{
		return make_err<WaveStream>(e.what());
	}
}

TPResult<WaveStream> WaveStream::create(const WaveFormat& waveFormat, std::string_view filepath)
{
	try
	{
		auto fs = FileStream::create(filepath, FileMode::Create, FileAccess::Write);
		TPtr<WaveStream> ptr = TPtr<WaveStream>(new WaveStream(waveFormat, std::move(fs)));
		return ptr;
	}
	catch (const std::exception& e)
	{
		return make_err<WaveStream>(e.what());
	}
}

TPResult<WaveStream> WaveStream::create(const WaveFormat& waveFormat, Stream* stream)
{
	try
	{
		TPtr<WaveStream> ptr = TPtr<WaveStream>(new WaveStream(waveFormat, stream));
		return ptr;
	}
	catch (const std::exception& e)
	{
		return make_err<WaveStream>(e.what());
	}
}
