#include"base_config.hpp"
#include"TResult.h"
#include"../ICapture.h"
#include"../STAWorker.h"
#include<Windows.h>
#include"../../WaveRingBuffer.h"

class IMMDevice;
class IAudioClient;
class IAudioCaptureClient;
class WASAPICapture : public ICapture
{
public:
	WASAPICapture(WaveFormat _waveFormat);
	~WASAPICapture();

	STAType initSTA(std::string_view id);

	TResult<void> init(std::string_view id);

	TResult<void> release();

	TResult<void> doCapture();

	STAType readNextPacket();

	TResult<void> captureAsync(WaveWriter* waveWriter, int maxRecordMills) override;

	TResult<void> waitCaptureDone() override;

	TResult<void> stopCapture() override;

	TResult<void> capture(WaveWriter* waveWriter, int maxRecordMills) override;

	CaptureState getCaptureState()
	{
		return this->captureState;
	}



private:
	WaveRingBuffer *ringBuffer = nullptr;
	WaveWriter* waveWriter = nullptr;
	long recordMills;
	STAWorker staWorker;
	CaptureState captureState = CaptureState::Stopped;
	IMMDevice* pDevice = nullptr;
	IAudioClient* pAudioClient = nullptr;
	IAudioCaptureClient* pCaptureClient = nullptr;
	WaveFormat waveFormat;
	HANDLE hEvent = nullptr;
	HANDLE hExit = nullptr;
	STAFuture captureFuture;
	
	long latencyMills = 100;


	UINT32 bufferFrameSize = 0;

};