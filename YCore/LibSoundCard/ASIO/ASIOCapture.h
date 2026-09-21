#pragma once
#include"base_config.hpp"
#include"TResult.h"
#include"../ICapture.h"
#include"../../WaveBuffer.h"
#include<vector>
#include<memory>

class ASIODriver;
struct _Client2;

//录音客户端:
// 构造时向 ASIODriver 注册(带各通道的单声道交换缓冲),
// 引擎收到通知后把输入环形区的采集数据写入这些缓冲, 上层通过 getBuffer 读取
class ASIOCapture final: public ICapture
{

public:
    //bufferMills: 每通道缓冲时长(ms), 0=默认(引擎环形容量), 与引擎缓冲独立
    ASIOCapture(ASIODriver* driver, int channelMask, int bufferMills = 0);
    ~ASIOCapture();

    ASIOCapture(const ASIOCapture&) = delete;
    ASIOCapture& operator=(const ASIOCapture&) = delete;
    ASIOCapture(ASIOCapture&&) = delete;
    ASIOCapture& operator=(ASIOCapture&&) = delete;


public:
    TResult<void> captureAsync(WaveWriter* waveWriter, int maxRecordMills) override;

    TResult<void> waitCaptureDone() override;

    TResult<void> stopCapture() override;

    TResult<void> capture(WaveWriter* waveWriter, int maxRecordMills) override;

    CaptureState getCaptureState()
    {
        return this->captureState;
    }

public:
    //按通道下标(在 _channels 中的顺序)访问与该客户端交换的音频缓冲
    WaveBuffer* getBuffer(int channelIndex);

public:
    std::vector<int> _channels;
    _Client2* _client = nullptr;   //注册到驱动的客户端聚合体
    ASIODriver* pDriver;

    CaptureState captureState = CaptureState::Stopped;

};
