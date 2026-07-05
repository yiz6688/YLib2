#pragma once
#include<string_view>
#include"../WaveReader.h"
#include<expected>
#include"../WaveFormat.h"

//播放状态
enum class PlaybackState
{
	Starting,  //正在启动

	Stopping,  //正在停止
	//停止播放
	Stopped,
	//播放中
	Playing
};


class IRender
{
public:

	virtual ~IRender() = default;

	//传递一个wav流开始播放
	virtual std::expected<void, std::string> playAsync(WaveReader* waveReader) = 0;

	virtual std::expected<void, std::string> waitPlayDone() = 0;

	virtual std::expected<void, std::string> stopPlay() = 0;

	virtual std::expected<void, std::string> play(WaveReader* waveReader) = 0;

	//获取当前播放器的状态
	virtual PlaybackState getPlaybackState() = 0;
	

};