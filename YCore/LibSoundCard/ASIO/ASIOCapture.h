#pragma once
#include"../ICapture.h"
#include"../../WaveRingBuffer.h"
#include<vector>

class ASIODriver;
class ASIOCapture final: public ICapture
{


public:
	ASIOCapture(ASIODriver* driver, int channelMask);
	~ASIOCapture();

	ASIOCapture(const ASIOCapture&) = delete;
	ASIOCapture(ASIOCapture&&) = default;

	ASIOCapture& operator=(const ASIOCapture&) = delete;
	ASIOCapture& operator=(ASIOCapture&&) = default;


public:
	std::expected<void, std::string> captureAsync(WaveWriter* waveWriter, int maxRecordMills) override;

	std::expected<void, std::string> waitCaptureDone() override;

	std::expected<void, std::string> stopCapture() override;

	std::expected<void, std::string> capture(WaveWriter* waveWriter, int maxRecordMills) override;

	CaptureState getCaptureState()
	{
		return this->captureState;
	}


public:

	std::vector<int> _channels;

	std::vector<WaveRingBuffer> _buffers;

	ASIODriver* pDriver;

	CaptureState captureState = CaptureState::Stopped;

};