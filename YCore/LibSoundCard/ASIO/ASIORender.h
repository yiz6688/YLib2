#pragma once
#include"base_config.hpp"
#include"../IRender.h"
#include"../../WaveBuffer.h"
#include<vector>
#include<memory>

class ASIODriver;
struct _Client2;

//播放客户端:
// 构造时向 ASIODriver 注册(带各通道的单声道交换缓冲),
// 上层通过 getBuffer 写入待播放数据, 引擎收到通知后对各通道混频并写入输出环形区
class ASIORender final : public IRender
{

public:
    //bufferMills: 每通道缓冲时长(ms), 0=默认(引擎环形容量), 与引擎缓冲独立
    ASIORender(ASIODriver* driver, int channelMask, int bufferMills = 0);
    ~ASIORender();

    ASIORender(const ASIORender&) = delete;
    ASIORender& operator=(const ASIORender&) = delete;
    ASIORender(ASIORender&&) = delete;
    ASIORender& operator=(ASIORender&&) = delete;


public:
    exp_ns::expected<void, std::string> playAsync(WaveReader* waveReader) override;

    exp_ns::expected<void, std::string> waitPlayDone() override;

    exp_ns::expected<void, std::string> stopPlay() override;

    exp_ns::expected<void, std::string> play(WaveReader* waveReader) override;

    PlaybackState getPlaybackState() override
    {
        return this->playbackState;
    }

public:
    //按通道下标(在 _channels 中的顺序)访问与该客户端交换的音频缓冲
    WaveBuffer* getBuffer(int channelIndex);

public:
    std::vector<int> _channels;
    _Client2* _client = nullptr;   //注册到驱动的客户端聚合体
    ASIODriver* pDriver;
    PlaybackState playbackState = PlaybackState::Stopped;

};
