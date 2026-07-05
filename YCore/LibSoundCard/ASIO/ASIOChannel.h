#pragma once
#include<string>
#include"../../RingBuffer.h"
#include"../../WaveFormat.h"


struct ASIOChannel
{

    
    //通道名称
    std::string name;
    //通道号
    int channel;
    //通道类型，输入还是输出
    int channelType;
    //采样点类型
    SampleType sampleType;
    //通道是否激活
    //int channelActive;
    //通道组
    //int channelGroup;


};