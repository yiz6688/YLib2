#pragma once
//#include"./asiosdk/asiosys.h"
//#include"./asiosdk/asio.h"
//#include"./asiosdk/iasiodrv.h"

#include<Windows.h>
#include<vector>
#include<mutex>
#include<condition_variable>
#include<atomic>
#include <list>
#include<initializer_list>
#include"./asiosdk/asio.h"
#include<expected>
#include"ASIOChannel.h"
#include"ASIOBuffer.h"

using std::initializer_list;



struct IASIO;
class ASIOObject;

class ASIODevice
{


public:

	ASIODevice(ASIOCallbacks* _callbacks, CLSID clsid);

	~ASIODevice();


public:
	
	//加载驱动，不创建缓冲区
	std::expected<void, std::string> loadInstance();
	
	//驱动初始化
	std::expected<void, std::string> deviceInit();   
	//释放驱动，析构时调用
	std::expected<void, std::string> deviceRelease();



	//使用特定的采样率打开声卡
	std::expected<void, std::string> driverOpen(int sampleRate);

	//设置通道的掩码，输入输出最多支持32个，通道bit位为1，表示启用,只能设置一次。
	std::expected<void, std::string> setChannelMask(unsigned inputMask, unsigned outputMask);

public:
	//驱动是否初始化的标志
	//bool has_init;

	//创建ASIO实例,
	// 无锁，内置相关逻辑检查，不会重复创建
	//当驱动不存在时，包括首次创建、上下电，或切换采样率后都要重新创建实例
	//加载成功返回true
	//加载失败确保指针为空
	//bool loadASIOInstance(long newSampleRate);
	//释放驱动，当创建失败或者异常发生时进行清理
	//void releaseInstance();

	std::expected<void, std::string> createBuffer();

	std::expected<void, std::string> getSampleRate();

	std::expected<void, std::string> supportSampleRate(long value);

	std::expected<void, std::string> setSampleRate(long value);

	std::expected<void, std::string> start();

	std::expected<void, std::string> stop();

	//底层驱动是否存活，通过读采样率判定
	//bool aliveing();

	//统计运行中的设备数量
	int get_running_device();

	


public:
	//std::mutex mtx;


	//每启动一次计数+1，每停止一次计数-1
	//计数为0时，调用驱动start，计数非0 +1返回
	//计数为1时 调用驱动stop，计数大于1 -1返回
	unsigned start_counter;  //运行计数器

	unsigned runningCounter;  //运行计数器


	//运行中的实例计数,每调用一次start，计数+1， 每调用一次stop，计数-1
	//调用start后，计数为1时 调用底层start
	//调用stop后， 计数为0时，调用底层stop
	//暂未使用
	//int runningNum;


	//驱动自旋锁
	//SpinLock driverLock;

	//缓冲区是否就绪标志
	bool bufferReady;

public:
	//当前使用的缓冲区大小
	long bufferSize;

	//驱动句柄
	IASIO* iasio;
	
	char driverName[32];
	//输入通道数
	long num_of_capture{ 0 };
	//输出通道数
	long num_of_render{ 0 };


	//缓冲区最小大小
	long bufferMinSize{ 0 };
	//缓冲区最大大小
	long bufferMaxSize{ 0 };
	//首选缓冲区大小
	long bufferPreferredSize{ 0 };
	//缓冲区精度
	long bufferGranularity{ 0 };
	//采样率
	long sampleRate{ 48000 };

	//采样类型
	SampleType sampleType;
	//采样位深
	int bitDepth;
	//环形缓冲区size，采样点数
	int ringBufferSize;

public:
	//输入通道信息
	std::vector<ASIOChannelInfo> inputChannels;
	//输出通道信息
	std::vector<ASIOChannelInfo> outputChannels;


	//选择的输入缓冲区
	//std::vector<ASIOBufferInfo> inputBuffers;
	//选择的输出缓冲区
	//std::vector<ASIOBufferInfo> outputBuffers;

	int inputMask = 0;
	int outputMask = 0;

	std::vector<ASIOBuffer> inputRing;
	std::vector<ASIOBuffer> outputRing;

	int allocFlag = 0;

private:
	ASIOObject* object;
	ASIOCallbacks* callbacks;


public:




	//ASIOChannel* getCapture(int channel);
	//
	//ASIOChannel* getRender(int channel);


	//ASIOChannel* captures;

	//ASIOChannel* renders;


public:

	void bufferProcess(int bufferIndex, ASIOBool directProcess);

	CLSID driverID;

};



