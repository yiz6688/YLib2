#include<windows.h>
#include"../../Encoding.h"
#include<mmdeviceapi.h>
#include<format>
#include"WASAPICapture.h"
#include<Audioclient.h>
#include<print>


static long ReftimesPerSec = 10000000;
static long ReftimesPerMillisec = 10000;

WASAPICapture::WASAPICapture(WaveFormat _waveFormat)
	:waveFormat(_waveFormat)
{}

WASAPICapture::~WASAPICapture()
{
	if (this->captureFuture.valid())
	{
		(void)this->stopCapture();
		this->captureFuture.wait();

		CloseHandle(this->hEvent);
		CloseHandle(this->hExit);
		this->hEvent = NULL;
		this->hExit = NULL;
	}
}

STAType WASAPICapture::initSTA(std::string_view id)
{
	std::println("调用initSTA");
	auto fu = this->staWorker.submit([this, id]
	{
		return this->init(id);
	});
	return fu.get();
}

std::expected<void, std::string> WASAPICapture::init(std::string_view id)
{

	IMMDeviceCollection* pCollection;
	IMMDeviceEnumerator* pEnumerator;
	HRESULT hr;


//无安全属性，手动重置 初始无信号
	this->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (this->hEvent == INVALID_HANDLE_VALUE)
	{
		return std::unexpected(std::format("{},hr={}", "CreateEvent fail", GetLastError()));
	}
	this->hExit = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (this->hExit == INVALID_HANDLE_VALUE)
	{
		return std::unexpected(std::format("{},hr={}", "CreateEvent fail", GetLastError()));
	}


	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);

	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "获取Enumerator失败", hr));
	}

	auto u16Id = Encoding::UTF8ToUTF16(id.data());

	hr =  pEnumerator->GetDevice(u16Id.c_str(),  &this->pDevice);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "获取Device失败", hr));
	}

	hr = this->pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "获取AudioClient失败", hr));
	}


	//初始化音频客户端
	//auto latencyRefTimes = this->latencyMilliseconds * 10000L;


	long hnsBufferDuration = this->latencyMills * 10000L;
	long hnsPeriodicity = 0;

	WAVEFORMATEX fmtEx = this->waveFormat.toWaveFormatEx();
	int streamFlags = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY | AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
	hr = this->pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, streamFlags, hnsBufferDuration, hnsPeriodicity, &fmtEx, NULL);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "AudioClient Initialize fail", hr));
	}

	REFERENCE_TIME latency_time;
	hr = pAudioClient->GetStreamLatency(&latency_time);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "AudioClient GetStreamLatency fail", hr));
	}

	hr = pAudioClient->GetBufferSize(&this->bufferFrameSize);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "AudioClient GetBufferSize fail", hr));
	}

	hr = pAudioClient->SetEventHandle(hEvent);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "AudioClient SetEventHandle fail", hr));
	}


	hr = this->pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "AudioClient GetService fail", hr));
	}


	return std::expected<void, std::string>();
}

std::expected<void, std::string> WASAPICapture::release()
{
	if (this->pCaptureClient)
	{
		this->pCaptureClient->Release();
		this->pCaptureClient = nullptr;
	}

	if (this->pAudioClient)
	{
		delete this->pAudioClient;
		this->pAudioClient = nullptr;

	}

	if (this->pDevice != nullptr)
	{
		this->pDevice->Release();
		this->pDevice = nullptr;
	}

	return std::expected<void, std::string>();
}




std::expected<void, std::string> WASAPICapture::doCapture()
{
	HRESULT hr;
	UINT32  numFramesPadding;

	auto bytesPerFrame = this->waveFormat.getBlockAlign();
	ResetEvent(this->hEvent);  //重置事件
	ResetEvent(this->hExit);  //重置事件

	hr = pAudioClient->Start();
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "AudioClient Start fail", hr));
	}

	this->captureState = CaptureState::Capturing;

	long actualDuration = (long)((double)ReftimesPerSec *
		this->bufferFrameSize / this->waveFormat.getSampleRate());
	int waitMilliseconds = (int)(3 * actualDuration / ReftimesPerMillisec);
	HANDLE handles[2] = { this->hEvent, this->hExit };

	while (this->captureState == CaptureState::Capturing)
	{
		DWORD dwSignalledIndex;
		hr = CoWaitForMultipleHandles(
					COWAIT_ALERTABLE,  // 允许在等待期间处理 APC（异步过程调用）
					waitMilliseconds,  // 超时时间：无限超时
					2,                 // 句柄数量
					handles,     // 句柄数组
					&dwSignalledIndex  // 输出：触发返回的句柄索引
				);

		if (hr == S_OK) {
			if(dwSignalledIndex == 0)  //信号事件
			{
				ResetEvent(this->hEvent);  //重置事件
			}else if(dwSignalledIndex == 1)  //退出事件
			{
				break;
			}
		}

		auto res = this->readNextPacket();
		if (!res)
		{
			break;
		}

	}

	hr = this->pAudioClient->Stop();
	if (FAILED(hr))
	{
		return std::unexpected(std::format("{},hr={}", "AudioClient Stop fail", hr));
	}
	this->captureState = CaptureState::Stopped;


	return STAType();
}

STAType WASAPICapture::readNextPacket()
{
	HRESULT hr;
	UINT32 packetSize = 0;
	UINT32 framesAvailable = 0;
	auto bytesPerFrame = this->waveFormat.getBlockAlign();


	while (true)
	{
		hr = this->pCaptureClient->GetNextPacketSize(&packetSize);
		if (FAILED(hr))	
		{
			return std::unexpected(std::format("{},hr={}", "AudioClient GetNextPacketSize fail", hr));
		}

		if (packetSize == 0)
		{
			break;
		}

		BYTE* pData;
		DWORD dwFlags;

		pCaptureClient->GetBuffer(
			&pData,
			&framesAvailable,
			&dwFlags, NULL, NULL);


		int bytesAvailable = framesAvailable * bytesPerFrame;
		//就方案是装不下的时候开始写入。
		
		// if not silence...
		if ((dwFlags & AUDCLNT_BUFFERFLAGS_SILENT) != AUDCLNT_BUFFERFLAGS_SILENT)
		{
			long mills = this->waveWriter->getTotalMills();
			long mills2 = this->recordMills - mills;
			long bytesWriteable = this->waveFormat.mills2Bytes(mills2);
			std::expected<long, std::string> writeResult;
			if(bytesWriteable > bytesAvailable)
			{
				writeResult = this->waveWriter->write((char*)pData, bytesAvailable);
			}else
			{
				writeResult = this->waveWriter->write((char*)pData, bytesWriteable);
				this->captureState = CaptureState::Stopping;
			}
			
			//this->ringBuffer->writeBytes((char*)pData, bytesAvailable);  //写入环形缓冲区
			//std::copy(pData, pData + bytesAvailable, this->recordBuffer + recordBufferOffset);  //拷贝到缓冲区中
		}
		else
		{
			//std::fill_n(recordBuffer + recordBufferOffset, bytesAvailable, 0);  //尾巴的这一段清空
			//写入0；
		}
		hr = this->pCaptureClient->ReleaseBuffer(framesAvailable);  //释放对应的缓冲区
		if (FAILED(hr))
		{
			return std::unexpected(std::format("{},hr={}", "AudioClient ReleaseBuffer fail", hr));
		}
	}


	//跳出来之后，检查缓冲区中是否有数据，有数据就写入流


	return std::expected<void, std::string>();
}

std::expected<void, std::string> WASAPICapture::captureAsync(WaveWriter* _waveWriter, int maxRecordMills)
{
	if (this->captureState == CaptureState::Starting || this->captureState == CaptureState::Capturing)
	{
		return std::unexpected("Capture is already running");
	}
	auto fmt = _waveWriter->getWaveFormat();
	if (fmt != this->waveFormat)
	{
		return std::unexpected("WaveFormat mismatch");
	}
	
	this->waveWriter = _waveWriter;
	this->recordMills = maxRecordMills;

	this->captureState = CaptureState::Starting;

	this->captureFuture = this->staWorker.submit(
		[this] {
		STAType result = this->doCapture();
		if (!result)
		{
			this->captureState = CaptureState::Stopped;
		}
		return result;
		});
	


	return std::expected<void, std::string>();
}

std::expected<void, std::string> WASAPICapture::waitCaptureDone()
{
	if (this->captureFuture.valid())
	{
		return this->captureFuture.get();
	}
	else
	{
		return STAType();
	}
}

std::expected<void, std::string> WASAPICapture::stopCapture()
{
	if (this->captureState != CaptureState::Stopped && this->captureState != CaptureState::Stopping)
	{
		this->captureState = CaptureState::Stopping; //标记为停止中
		SetEvent(this->hExit);  //触发退出事件
	}
	return std::expected<void, std::string>();
}

std::expected<void, std::string> WASAPICapture::capture(WaveWriter* waveWriter, int maxRecordMills)
{
	auto result = this->captureAsync(waveWriter, maxRecordMills);
	if (!result)
	{
		return result;
	}
	
	result = this->waitCaptureDone();
	return result;
}
