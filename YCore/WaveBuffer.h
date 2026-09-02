#pragma once
#include"WaveFormat.h"
#include"myType.h"
#include"RingBuffer2.h"
#include<vector>
#include<cstring>




struct Sample
{
    union
    {
        double* pd;
        float*  pf;
        int*    pi;
        int24*  pt;
        short*  ps;
        char*   raw;
    };


    SampleType _type;

    int _chnInx;

};


struct WaveMix
{

public:

    WaveMix()
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



    int writeBytes(char* ptr, int byteSize);
    int readBytes(char* ptr, int byteSize);

private:

    template<typename F>
    //int toFloat32(char* src, F* dest, int sampleNum, int chnIndex)
    int toFloat32(char *src, F *dest, int sampleNum, int chnIndex)
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
                dest++;
            }
        }
        else if(this->_type == SampleType::IEEE32)
        {
            float value = 0.0f;
            for(int i=0; i<nFrames; i++)
            {
                std::memcpy(&value, src, 4);
                *dest = value;
                src += this->_frameSize; //起始指针
                dest++;
            }
        }else if(this->_type == SampleType::INT32)
        {
            int value = 0;
            F coeff = -1.0 / (std::numeric_limits<int>::min)();
            for(int i=0; i<nFrames; i++)
            {
                value = (src[0] & 0xFF) | ((src[1] & 0xFF) << 8) | ((src[2] & 0xFF) << 16) | ((src[3] & 0xFF) << 24);
                *dest = value * coeff;
                src += this->_frameSize; //起始指针
                dest++;
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
                dest++;
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
                dest++;
            }
        }else
        {
            return 0;
        }
        
        return nFrames;
    }

    template<typename F>
    int fromFloat32(F* src, char* dest, int sampleNum, int chnIndex)
    //int WaveBuffer::fromFloat32(float *src, char *dest, int sampleNum, int chnIndex)
    {
        dest = dest + chnIndex * this->_byteDepth;  //起始指针位置
        int nFrames = sampleNum;

        if(this->_type == SampleType::IEEE64)
        {
            double value = 0.0;
            for(int i=0; i<nFrames; i++)
            {
                value = src[i];
                std::memcpy(dest, &value, 8);
                src++;
                dest += this->_frameSize; //起始指针
            }
        }
        else if(this->_type == SampleType::IEEE32)
        {
            float value = 0.0f;
            for(int i=0; i<nFrames; i++)
            {
                value = src[i];
                std::memcpy(dest, &value, 4);
                src++;
                dest += this->_frameSize; //起始指针
            }
        }else if(this->_type == SampleType::INT32)
        {
            int value = 0;
            for(int i=0; i<nFrames; i++)
            {
                double v = std::clamp(static_cast<double>(*src), -1.0, 1.0);
                value = static_cast<int>(std::round(v * static_cast<double>((std::numeric_limits<int>::max)())));
                std::memcpy(dest, &value, 4);
                src++;
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
                src++;
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
                src++;
                dest += this->_frameSize; //起始指针
            }
        }else
        {
            return 0;
        }
        
        return nFrames;
    }



private:

    SampleType _type; //类型
	int _byteDepth;    //采样位宽
    int _chnNum;       //通道数
    int _frameSize;   //大小， 通道*位宽
	std::unique_ptr<ByteRing> _pRing;

};