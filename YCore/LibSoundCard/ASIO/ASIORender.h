#pragma once
#include"../IRender.h"
#include<vector>
#include"../../WaveRingBuffer.h"

class ASIODriver;
class ASIORender final : public IRender
{

public:
	ASIORender(ASIODriver* driver, int channelMask);
	~ASIORender() = default;


public:
	std::expected<void, std::string> playAsync(WaveReader* waveReader) override;

	std::expected<void, std::string> waitPlayDone() override;

	std::expected<void, std::string> stopPlay() override;

	std::expected<void, std::string> play(WaveReader* waveReader) override;

	PlaybackState getPlaybackState() override
	{
		return this->playbackState;
	}


public:
	std::vector<int> _channels;
	std::vector<WaveRingBuffer> _buffers;
	ASIODriver* pDriver;
	PlaybackState playbackState = PlaybackState::Stopped;

};