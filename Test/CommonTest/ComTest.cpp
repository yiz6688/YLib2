#include<print>
#include<windows.h>
#include"../../YCore/WinUtils.h"
#include<cstddef>
#include<vector>
#include<iostream>
#include"myType.h"
#include"FileStream.h"
#include"RingBuffer2.h"
#include"WaveBuffer.h"

using namespace std;



void fileTest()
{
    



}

void ringTest()
{
    ByteRing ring(1024);
    char buffer[128] = {};
    char buffer2[128] = {};

    ring.write(buffer, 128);
    ring.read(buffer2, 128);

}


void waveBufferTest()
{

    int chnNum = 4;
    int sampleNum = 12;
    int byteDepth = 2;
    SampleType type = SampleType::INT16;
    using I = short;

    if(type == SampleType::INT32)
    {
        byteDepth = 4;
    }else if(type == SampleType::INT24)
    {
        byteDepth = 3;
    }else if(type == SampleType::INT16)
    {
        byteDepth = 2;
    }

    



    vector<I> total;
    vector<vector<I>> lst;

    
    for(int n = 0; n < sampleNum; n++)
    {
        for(int ch = 0; ch< chnNum; ch++)
        {
            int val = n * pow(10, ch) + 1;
            //lst[ch].push_back(val);
            total.push_back(val);
        }
    }


    WaveBuffer wbuf(type, sampleNum, chnNum);
    WaveBuffer wbuf2(type, sampleNum, chnNum);
    char* src = reinterpret_cast<char*>(total.data());

    for(int k = 0; k<30; k++)
    {
        int size = wbuf.writeBytes(src, sampleNum * chnNum * byteDepth);
        std::println("写入字节数:{}", size);

        
        WaveMix mix;
        lst.resize(chnNum);
        for(int i=0; i<chnNum; i++)
        {
            Sample s;
            vector<I> vec(sampleNum);
            s.ps = vec.data(); s._type = type; s._chnInx = i;
            //lst.push_back(std::move(vec));
            lst[i] = std::move(vec);
            mix.add(s);
        }

        int nFrame = wbuf.readRaw(mix, sampleNum + 3);
        std::println("读取的帧数:{}", nFrame);

        for(int ch = 0; ch< chnNum; ch++)
        {
            auto& vec = lst[ch];
            for(int n = 0; n < sampleNum; n++)
            {
                int val = n * pow(10, ch) + 1;
                if(vec[n] != val)
                {
                    std::println("通道:{}, 采样点:{}, 读取值:{}, 理论值:{}", ch, n, (int)vec[n], val);
                    return;
                }
            }
        }
        std::println("{} 读取成功",k );

        nFrame = wbuf2.writeRaw(mix, nFrame);
        std::println("联合写入:{}", nFrame);
        vector<I> total2(total.size());
        char* ptr1 = reinterpret_cast<char*>(total2.data());
        size = wbuf2.readBytes(ptr1, total2.size() * byteDepth);
        for(int i=0; i< total2.size(); i++)
        {
            if(total[i] != total2[i])
            {
                std::println("读取值:{}, 理论值:{}", (int)total2[i], (int)total[i]);
                return;
            }
        }

        std::println("读取字节数:{}, 核对成功", size);
    }
    

    return;
}

bool equal(float v1, float v2)
{
    if(v1 > v2 - 0.0001 && v1 < v2 + 0.0001)
    {
        return true;
    }else
    {
        return false;
    }
}

void waveBufferTest2()
{

    int chnNum = 4;
    int sampleNum = 12;
    int byteDepth = 4;
    SampleType type = SampleType::INT16;
    using I = short;

    if(type == SampleType::INT32)
    {
        byteDepth = 4;
    }else if(type == SampleType::INT24)
    {
        byteDepth = 3;
    }else if(type == SampleType::INT16)
    {
        byteDepth = 2;
    }

    



    vector<I> total;
    vector<vector<float>> lst;

    
    for(int n = 0; n < sampleNum; n++)
    {
        for(int ch = 0; ch< chnNum; ch++)
        {
            float val = 0.05 * n - ch * 0.1;
            //lst[ch].push_back(val);
            total.push_back(val * (std::numeric_limits<I>::max)());
        }
    }


    WaveBuffer wbuf(type, sampleNum, chnNum);
    WaveBuffer wbuf2(type, sampleNum, chnNum);
    char* src = reinterpret_cast<char*>(total.data());

    for(int k = 0; k<30; k++)
    {
        int size = wbuf.writeBytes(src, sampleNum * chnNum * byteDepth);
        std::println("写入字节数:{}", size);

        
        WaveMix mix;
        lst.resize(chnNum);
        for(int i=0; i<chnNum; i++)
        {
            Sample s;
            vector<float> vec(sampleNum);
            s.pf = vec.data(); s._type = SampleType::IEEE32; s._chnInx = i;
            //lst.push_back(std::move(vec));
            lst[i] = std::move(vec);
            mix.add(s);
        }

        int nFrame = wbuf.readSample(mix, sampleNum + 3);
        std::println("读取的帧数:{}", nFrame);

        for(int ch = 0; ch< chnNum; ch++)
        {
            auto& vec = lst[ch];
            for(int n = 0; n < sampleNum; n++)
            {
                float val = 0.05 * n - ch * 0.1;
                if(equal(vec[n], val) == false)
                {
                    std::println("通道:{}, 采样点:{}, 读取值:{}, 理论值:{}", ch, n, vec[n], val);
                    return;
                }
            }
        }
        std::println("{} 读取成功",k );
    }
    

    return;
}


int main()
{
    //println("{}", "comm");

    // wstring path = L"D:\\12/fdfdfd/34\\drerer";
    // auto rxx = WinUtils::makeDirs(path);
    // if(!rxx)
    // {
    //     auto error = rxx.error();
    //     std::println("{}", error);
    // }


    waveBufferTest2();


}