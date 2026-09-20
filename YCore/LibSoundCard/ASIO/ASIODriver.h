#pragma once
#include<string>
#include"../STAWorker.h"
#include"AsioDevice.h"
#include"../../RingBuffer.h"
#include<vector>
#include"ASIOCapture.h"
#include"ASIORender.h"
#include"AsioDevice.h"
#include<memory>
#include<expected>
#include"../../StopWatch.h"

struct ASIOCallbacks;




//聚合每一个具体通道的内容
struct _Client
{
public:
	std::vector<char> buffers;  //总空间
	std::vector<WaveBuffer*> wbs;
	int channel;
	int type;  //输入or输出

};


//每一个具体对象的内容，包含各个通道
struct _Client2
{


public:
	std::vector<char> buffers;     //合并的整块缓冲区
	std::vector<WaveBuffer*> wbs;  //区分的不同音频缓冲
	std::vector<int> chs;         //对应的通道

	int type; //输入或输出
};



//ASIO通道的类
struct _Channel
{

	int channel;
	int type;  //输入or输出
	std::vector<WaveBuffer*> wbs;  //区分的不同音频缓冲
	//硬件乒乓缓冲区。
};



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

	void removeClient(_Client2* client);

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

	TResult<void> Initialize(unsigned inputMask, unsigned outputMask);

	TResult<void> Release();

private:
	static unsigned __stdcall threadProc(void* param)
	{
		ASIODriver* driver = reinterpret_cast<ASIODriver*>(param);
		driver->processor();
		return 0;
	}

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
	std::vector<ASIOCapture*> captureLsts;
	//播放列表
	std::vector<ASIORender*> renderLsts;


	//需要一个临时缓冲区，用来存储数据，是否需要对等

	//输入 输出缓存的缓冲区
	
	//std::vector<RingBuffer> _inputBuffers;
	//std::vector<RingBuffer> _outputBuffers;

	Stopwatch sw;

	int processFlag = false;

	//std::future<void> fu;
	HANDLE hThread = INVALID_HANDLE_VALUE;

private:
	//通知缓冲区大小
	int notifyBufferSize = 0;
	//最大缓存缓冲区大小
	int maxBufferSize = 0;
	//缓冲区计数
	//int bufferCounter = 0;

	//可用帧数
	std::atomic<int> _validFrameNum = 0;



	std::vector<_Client*>  chClient; //通道客户端，完全体的客户端

	std::vector<_Client*> clientQueue;  //缓冲区通道队列， 增/删的都在这里面。 由引擎线程处理。

	std::vector<_Client2*> iClient_2;
	std::vector<_Client2*> oClient_2;

	
	std::vector<_Client2*> clients; //客户端列表

	std::vector<_Client2*> addLst;   //增加列表
	std::vector<_Client2*> removeLst;  //删除列表


	std::vector<char> _iCache;  //输入通道的缓存
	std::vector<char> _oCache;  //输出通道的缓存

	std::vector<_Channel> _iWbs;  //输入通道的缓存

	std::vector<_Channel> chViews;  //通道视图

};