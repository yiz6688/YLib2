#include"base_config.hpp"
#include"TResult.h"
#include"../IRender.h"
#include"../STAWorker.h"
#include<Windows.h>
#include"../../WaveRingBuffer.h"




class IMMDevice;
class IAudioClient;
class IAudioRenderClient;
class WASAPIRender : public IRender
{
public:
	WASAPIRender(WaveFormat fmt);
	~WASAPIRender();


	STAType initSTA(std::string_view id);

	TResult<void> init(std::string_view id);

	TResult<void> release();

	TResult<void> doPlay();

	STAType fillBuffer(int frameSize);

	TResult<void> playAsync(WaveReader* waveReader) override;

	TResult<void> waitPlayDone() override;

	TResult<void> stopPlay() override;

	TResult<void> play(WaveReader* waveReader) override;

	PlaybackState getPlaybackState() override
	{
		return this->playbackState;
	}



private:
	WaveRingBuffer* ringBuffer = nullptr;
	WaveReader* waveReader = nullptr;
	STAWorker staWorker;
	PlaybackState playbackState = PlaybackState::Stopped;
	IMMDevice* pDevice = nullptr;
	IAudioClient* pAudioClient = nullptr;
	IAudioRenderClient* pRenderClient = nullptr;
	WaveFormat waveFormat;
	HANDLE hEvent = nullptr;
	HANDLE hExit = nullptr;
	STAFuture renderFuture;

	long latencyMills = 100;

	UINT32 bufferFrameSize = 0;

	WaveReader *reader = nullptr;
};