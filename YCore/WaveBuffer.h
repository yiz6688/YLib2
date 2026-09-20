#pragma once
#include"WaveFormat.h"
#include"myType.h"
#include"RingBuffer2.h"
#include<vector>
#include<cstring>
#include<limits>
#include<algorithm>
#include<type_traits>




struct Sample
{
    // union
    // {
    //     double* pd;
    //     float*  pf;
    //     int*    pi;
    //     int24*  pt;
    //     short*  ps;
    //     char*   raw;
    // };

public:
    Sample(short* ptr, int chInx=-1)
        :_raw(reinterpret_cast<char*>(ptr)), _chnInx(chInx), _type(SampleType::INT16)
    {}

    Sample(int24* ptr, int chInx=-1)
        :_raw(reinterpret_cast<char*>(ptr)), _chnInx(chInx), _type(SampleType::INT24)
    {}  

    Sample(int* ptr, int chInx=-1)
        :_raw(reinterpret_cast<char*>(ptr)), _chnInx(chInx), _type(SampleType::INT32)
    {}

    Sample(float* ptr, int chInx=-1)
        :_raw(reinterpret_cast<char*>(ptr)), _chnInx(chInx), _type(SampleType::IEEE32)
    {}

    Sample(double* ptr, int chInx=-1)
        :_raw(reinterpret_cast<char*>(ptr)), _chnInx(chInx), _type(SampleType::IEEE64)
    {}

    Sample(char* ptr, int chInx=-1)
        :_raw(ptr), _chnInx(chInx), _type(SampleType::UNKNOWN)
    {}

    char* _raw;

    int _chnInx;

    SampleType _type;



};


//纯字节通道搬运描述: 与 Sample 对齐(但无类型, 也就没有类型一致性检查), 按字节计数
struct ChannelBytes
{
    char* raw;      //目标(读)/源(写)缓冲
    int   byteSize; //该通道字节数
    int   chnInx;   //指定通道
};


struct WaveMix
{

public:

    WaveMix()
        :_type{ SampleType::UNKNOWN }
    {}

    WaveMix(SampleType type)
        :_type{type}
    {}

public:
    void add(Sample& value)
    {
        if(this->_datas.empty())
        {
            this->_datas.push_back(value);
            this->_type = value._type;
        }else
        {
            if(this->_type != value._type)
            {
                return;
            }
            auto iter = this->_datas.end();
            while(true)
            {
                iter--;
                if(iter->_chnInx == value._chnInx)
                {
                    return;
                }else if(iter->_chnInx < value._chnInx)
                {
                    iter++; //修正位置
                    break;
                }

                if(iter == this->_datas.begin())
                {
                    break;
                }
            }
            this->_datas.insert(iter, value);

        }
    }


    std::vector<Sample> _datas;
    SampleType _type;
};





class WaveBuffer
{

public:
    WaveBuffer(SampleType type, int sampleLen, int chnNum);

    ~WaveBuffer();


public:


    int readSample(WaveMix& mix, int sampleNum);
    int writeSample(WaveMix& mix, int sampleNum);

    int readRaw(WaveMix& mix, int sampleNum);
    int writeRaw(WaveMix& mix, int sampleNum);

    //交织原始数据读写: 以 Sample 描述缓冲(入参), 直接读写交织字节
    int readRaw(Sample& sample, int sampleNum);
    int writeRaw(Sample& sample, int sampleNum);

    //纯字节单通道: 去交织读/交织写指定通道(byteSize 为该通道字节数), 返回搬运的字节数
    int readChannelBytes(int ch, char* dest, int byteSize);
    int writeChannelBytes(int ch, const char* src, int byteSize);

    //纯字节按需多通道: 一次锁定按指定通道去交织读/交织写(vector<ChannelBytes>), 返回每通道字节数
    int readChannelsBytes(const std::vector<ChannelBytes>& channels);
    int writeChannelsBytes(const std::vector<ChannelBytes>& channels);



    int writeBytes(char* ptr, int byteSize);
    int readBytes(char* ptr, int byteSize);

    //交织浮点读写(泛型): 直接以交织(F=float/double)形式读写环形区, 不做拆通道
    template<typename F>
    int readFloat(F* buffer, int sampleNum);

    template<typename F>
    int writeFloat(F* buffer, int sampleNum);

    //交织整型/原始字节读写(类似 WaveRingBuffer, 内部格式与目标格式自动转换):
    // 支持从任意内部格式(INT16/INT24/INT32/IEEE32/IEEE64)转换到目标采样格式
    int readInt16(short* buffer, int nSample);      //交织读 -> int16
    int writeInt16(short* buffer, int nSample);     //交织写 <- int16
    int readInt24(int* buffer, int nSample);        //交织读 -> int32容器(24位范围)
    int writeInt24(int* buffer, int nSample);       //交织写 <- int32容器(24位范围)
    int readInt32(int* buffer, int nSample);        //交织读 -> int32(32位范围)
    int writeInt32(int* buffer, int nSample);       //交织写 <- int32(32位范围)
    int readInt24Bytes(char* buffer, int nSample);  //交织读 -> 3字节小端原始字节
    int writeInt24Bytes(char* buffer, int nSample); //交织写 <- 3字节小端原始字节

    //查询: 可读/可写采样点(帧 × 通道数), 容量(采样点)
    int getReadableSample();
    int getWriteableSample();
    int getCapacity();

    std::span<char> getReadBuffer(int perChSize);
    int releaseReadBuffer();

    std::span<char> getWriteBuffer(int perChSize);
    int releaseWriteBuffer();


private:

    template<typename F>
    //int toFloat32(char* src, F* dest, int sampleNum, int chnIndex)
    int toFloat32(char *src, F *dest, int sampleNum, int chnIndex, int stride = 1)
    {
        src = src + chnIndex * this->_byteDepth;  //起始指针位置
        int nFrames = sampleNum;

        if(this->_type == SampleType::IEEE64)
        {
            double value = 0.0;
            for(int i=0; i<nFrames; i++)
            {
                std::memcpy(&value, src, 8);
                *dest = value;
                src += this->_frameSize; //起始指针
                dest += stride;
            }
        }
        else if(this->_type == SampleType::IEEE32)
        {
            float value = 0.0f;
            for(int i=0; i<nFrames; i++)
            {
                std::memcpy(&value, src, 4);

                *dest = value;
                src += this->_frameSize;
                dest += stride;
            }
        }
        else if(this->_type == SampleType::INT32)
        {
            int value = 0;
            F coeff = -1.0 / (std::numeric_limits<int>::min)();
            for(int i=0; i<nFrames; i++)
            {
                value = (src[0] & 0xFF) | ((src[1] & 0xFF) << 8) | ((src[2] & 0xFF) << 16) | ((src[3] & 0xFF) << 24);
                *dest = value * coeff;
                src += this->_frameSize; //起始指针
                dest += stride;
            }
        }else if(this->_type == SampleType::INT24)
        {
            int value = 0;
            F coeff = -1.0 / (std::numeric_limits<int24>::min)();
            for(int i=0; i<nFrames; i++)
            {
                value = ((src[0] & 0xFF) << 8) | ((src[1] & 0xFF) << 16) | ((src[2] & 0xFF) << 24);
                value >>= 8;
                *dest = value * coeff;
                src += this->_frameSize; //起始指针
                dest += stride;
            }
        }else if(this->_type == SampleType::INT16)
        {
            int value = 0;
            F coeff = -1.0 / (std::numeric_limits<short>::min)();
            for(int i=0; i<nFrames; i++)
            {
                value = ((src[0] & 0xFF) << 16) | ((src[1] & 0xFF) << 24);
                value >>= 16;
                *dest = value * coeff;
                src += this->_frameSize; //起始指针
                dest += stride;
            }
        }else
        {
            return 0;
        }
        
        return nFrames;
    }

    template<typename F>
    int fromFloat32(F* src, char* dest, int sampleNum, int chnIndex, int stride = 1)
    //int WaveBuffer::fromFloat32(float *src, char *dest, int sampleNum, int chnIndex)
    {
        dest = dest + chnIndex * this->_byteDepth;  //起始指针位置
        int nFrames = sampleNum;

        if(this->_type == SampleType::IEEE64)
        {
            double value = 0.0;
            for(int i=0; i<nFrames; i++)
            {
                value = *src;
                std::memcpy(dest, &value, 8);
                src += stride;
                dest += this->_frameSize; //起始指针
            }
        }
        else if(this->_type == SampleType::IEEE32)
        {
            float value = 0.0f;
            for(int i=0; i<nFrames; i++)
            {
                value = *src;
                std::memcpy(dest, &value, 4);
                src += stride;
                dest += this->_frameSize; //起始指针
            }
        }else if(this->_type == SampleType::INT32)
        {
            int value = 0;
            for(int i=0; i<nFrames; i++)
            {
                //用 double 计算:float 精度无法精确表示 2^31-1,1.0f*INT_MAX 会舍入成 2^31 而溢出
                double v = std::clamp(static_cast<double>(*src), -1.0, 1.0);
                value = static_cast<int>(std::round(v * static_cast<double>((std::numeric_limits<int>::max)())));
                std::memcpy(dest, &value, 4);
                src += stride;
                dest += this->_frameSize; //起始指针
            }
        }else if(this->_type == SampleType::INT24)
        {
            int24 value = 0;
            for(int i=0; i<nFrames; i++)
            {
                double v = std::clamp(static_cast<double>(*src), -1.0, 1.0);
                value = static_cast<int>(std::round(v * static_cast<double>((std::numeric_limits<int24>::max)())));
                std::memcpy(dest, &value, 3);
                src += stride;
                dest += this->_frameSize; //起始指针
            }
        }else if(this->_type == SampleType::INT16)
        {
            short value = 0;
            for(int i=0; i<nFrames; i++)
            {
                double v = std::clamp(static_cast<double>(*src), -1.0, 1.0);
                value = static_cast<int>(std::round(v * static_cast<double>((std::numeric_limits<short>::max)())));
                std::memcpy(dest, &value, 2);
                src += stride;
                dest += this->_frameSize; //起始指针
            }
        }else
        {
            return 0;
        }
        
        return nFrames;
    }


private:
    //内部格式 -> 目标整型(逐通道去交织, stride=目标通道数, 定义于WaveBuffer.cpp)
    void toInt16(char* src, short* dest, int nFrames, int chnIndex, int stride);
    void fromInt16(short* src, char* dest, int nFrames, int chnIndex, int stride);
    void toInt24(char* src, int* dest, int nFrames, int chnIndex, int stride);
    void fromInt24(int* src, char* dest, int nFrames, int chnIndex, int stride);
    void toInt32(char* src, int* dest, int nFrames, int chnIndex, int stride);
    void fromInt32(int* src, char* dest, int nFrames, int chnIndex, int stride);
    void toInt24Bytes(char* src, char* dest, int nFrames, int chnIndex, int stride);
    void fromInt24Bytes(char* src, char* dest, int nFrames, int chnIndex, int stride);


private:

    SampleType _type; //类型
	int _byteDepth;    //采样位宽
    int _chnNum;       //通道数
    int _frameSize;   //大小， 通道*位宽
	std::unique_ptr<ByteRing> _pRing;

};

//交织浮点读取(泛型): 存储(交织) -> F(float/double) 交织输出, 返回读出的采样数
template<typename F>
int WaveBuffer::readFloat(F* buffer, int sampleNum)
{
    if (buffer == nullptr || sampleNum <= 0 || this->_chnNum <= 0)
    {
        return 0;
    }
    int frames = sampleNum / this->_chnNum;
    if (frames <= 0)
    {
        return 0;
    }
    int rdFrames = 0;
    while (rdFrames < frames)
    {
        int byteSize = (frames - rdFrames) * this->_frameSize;
        auto buf = this->_pRing->getReadBuffer(byteSize);
        if (buf.size() == 0)
        {
            break;
        }
        int nFrames = buf.size() / this->_frameSize;
        //快速路径: 类型完全匹配(IEEE32->float / IEEE64->double)时无需转换, 整块拷贝
        if constexpr (std::is_same_v<F, float>)
        {
            if (this->_type == SampleType::IEEE32)
            {
                std::memcpy(buffer + static_cast<long>(rdFrames) * this->_chnNum, buf.data(), buf.size());
            }
            else
            {
                for (int ch = 0; ch < this->_chnNum; ch++)
                {
                    this->toFloat32(buf.data(), buffer + static_cast<long>(rdFrames) * this->_chnNum + ch,
                        nFrames, ch, this->_chnNum);
                }
            }
        }
        else
        {
            if (this->_type == SampleType::IEEE64)
            {
                std::memcpy(buffer + static_cast<long>(rdFrames) * this->_chnNum, buf.data(), buf.size());
            }
            else
            {
                for (int ch = 0; ch < this->_chnNum; ch++)
                {
                    this->toFloat32(buf.data(), buffer + static_cast<long>(rdFrames) * this->_chnNum + ch,
                        nFrames, ch, this->_chnNum);
                }
            }
        }
        rdFrames += nFrames;
        this->_pRing->releaseReadBuffer();
    }
    return rdFrames * this->_chnNum;
}

//交织浮点写入(泛型): F(float/double) 交织输入 -> 存储(交织), 返回写入的采样数
template<typename F>
int WaveBuffer::writeFloat(F* buffer, int sampleNum)
{
    if (buffer == nullptr || sampleNum <= 0 || this->_chnNum <= 0)
    {
        return 0;
    }
    int frames = sampleNum / this->_chnNum;
    if (frames <= 0)
    {
        return 0;
    }
    int wrFrames = 0;
    while (wrFrames < frames)
    {
        int byteSize = (frames - wrFrames) * this->_frameSize;
        auto buf = this->_pRing->getWriteBuffer(byteSize);
        if (buf.size() == 0)
        {
            break;
        }
        int nFrames = buf.size() / this->_frameSize;
        //快速路径: 类型完全匹配(float->IEEE32 / double->IEEE64)时无需转换, 整块拷贝
        if constexpr (std::is_same_v<F, float>)
        {
            if (this->_type == SampleType::IEEE32)
            {
                std::memcpy(buf.data(), buffer + static_cast<long>(wrFrames) * this->_chnNum, buf.size());
            }
            else
            {
                for (int ch = 0; ch < this->_chnNum; ch++)
                {
                    this->fromFloat32(buffer + static_cast<long>(wrFrames) * this->_chnNum + ch,
                        buf.data(), nFrames, ch, this->_chnNum);
                }
            }
        }
        else
        {
            if (this->_type == SampleType::IEEE64)
            {
                std::memcpy(buf.data(), buffer + static_cast<long>(wrFrames) * this->_chnNum, buf.size());
            }
            else
            {
                for (int ch = 0; ch < this->_chnNum; ch++)
                {
                    this->fromFloat32(buffer + static_cast<long>(wrFrames) * this->_chnNum + ch,
                        buf.data(), nFrames, ch, this->_chnNum);
                }
            }
        }
        wrFrames += nFrames;
        this->_pRing->releaseWriteBuffer();
    }
    return wrFrames * this->_chnNum;
}



