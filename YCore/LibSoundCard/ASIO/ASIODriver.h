#pragma once
#include<string>
#include"../STAWorker.h"
#include"AsioDevice.h"
#include"../../RingBuffer.h"
#include<vector>
#include"ASIOCapture.h"
#include"ASIORender.h"
#include"ASIOBuffer.h"
#include"AsioDevice.h"
#include<memory>
#include<expected>
#include"ASIOBuffer.h"
#include"../../StopWatch.h"

struct ASIOCallbacks;

class ASIODriver
{

public:
	ASIODriver(ASIOCallbacks* callbacks, CLSID clsid);
	~ASIODriver();


public:

	std::expected<void, std::string> create(CLSID clsid);


public:
	//获取播放客户端
	std::expected<ASIORender*, std::string> createRender(int channelMask);
	//获取录音客户端
	std::expected<ASIOCapture*, std::string> createCapture(int channelMask);


private:
	void processor();



public:
	//ASIO回调函数，底层驱动调用
	void bufferProcess(long doubleBufferIndex, ASIOBool directProcess);


private:
	//打开驱动
	STAType driverOpen();


public:


	STAType createBuffer();

	STAType start();

	STAType stop();

	STAType setChannelMask(unsigned inputMask, unsigned outputMask);


		//获取采样率
	TResult<int> getSampleRate();
	//设置采样率
	TResult<void> setSampleRate(long sampleRate);

		//获取录音器数量
	int getCaptureCount();
	//获取播放器数量
	int getRenderCount();

	std::string getCaptureName(int channel);

	std::string getRenderName(int channel);




public:



	static std::expected<ASIODriver*, std::string> createDriver(CLSID clsid);

	static std::expected<void, std::string> releaseDriver(ASIODriver* driver);


public:


	std::atomic<int> cnt;

	//缓存计数，  计数等于通知值的时候，发起通知。
	int counter = 0;

	std::condition_variable cv;
	std::mutex mtx;

	int number = 0;   //当前统计的计数量
	int maxNum = 100; //最大的计数量

	//驱动偏移量
	int driverOffset = 0;

	//int refCounter;  //引用计数，记录当前有多少个录音或播放实例在使用驱动

	CLSID asioID;

	//设备驱动
	std::unique_ptr<ASIODevice> pAsioDevice;
	//单线程调度器
	STAWorker staWorker;
	//录音列表
	std::vector<std::unique_ptr<ASIOCapture>> captureLsts;
	//播放列表
	std::vector<std::unique_ptr<ASIORender>> renderLsts;


	std::vector<WaveRingBuffer*> waveInputBuffers;
	std::vector<WaveRingBuffer*> waveOutputBuffers;

	//需要一个临时缓冲区，用来存储数据，是否需要对等

	//输入 输出缓存的缓冲区
	
	std::vector<RingBuffer> _inputBuffers;
	std::vector<RingBuffer> _outputBuffers;

	Stopwatch sw;

	int processFlag = false;

	std::future<void> fu;

private:
	//通知缓冲区大小
	int notifyBufferSize = 0;
	//最大缓存缓冲区大小
	int maxBufferSize = 0;
	//缓冲区计数
	//int bufferCounter = 0;

	std::atomic<int> _bufferCounter = 0;
	//设备缓冲区,采样点数
	int deviceBufferSize = 0;
	//设备缓冲区，字节数
	int deviceByteSize = 0;

};