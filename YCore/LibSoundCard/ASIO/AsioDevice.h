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
#include"TResult.h"
#include"WaveFormat.h"
#include<span>

using std::initializer_list;



struct IASIO;
class ASIOObject;



struct ASIOBuffer2
{
 
public:
    ASIOBuffer2(int _channel, char* buf)
        :channel{_channel}
    {
       this->_buf = buf; 
    }




	std::span<char> getBuffer(int bufferIndex)
    {
        return std::span<char>(_buf, 100);
    }

    //通道号
    int channel;

    char* _buf;

    void* buffers[2] {nullptr, nullptr};
};




class ASIODevice
{


public:

	ASIODevice(ASIOCallbacks* _callbacks, CLSID clsid);

	~ASIODevice();


public:
	
	//加载驱动，不创建缓冲区
	TResult<void> loadInstance();
	
	//驱动初始化
	TResult<void> deviceInit();   
	//释放驱动，析构时调用
	TResult<void> deviceRelease();



	//使用特定的采样率打开声卡
	TResult<void> driverOpen(int sampleRate);

	//设置通道的掩码，输入输出最多支持32个，通道bit位为1，表示启用,只能设置一次。
	TResult<void> setChannelMask(unsigned inputMask, unsigned outputMask);

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

	TResult<void> getHaParam();


public:
	//std::mutex mtx;


	//每启动一次计数+1，每停止一次计数-1
	//计数为0时，调用驱动start，计数非0 +1返回
	//计数为1时 调用驱动stop，计数大于1 -1返回
	//unsigned start_counter;  //运行计数器

	//unsigned runningCounter = 0;  //运行计数器


	//运行中的实例计数,每调用一次start，计数+1， 每调用一次stop，计数-1
	//调用start后，计数为1时 调用底层start
	//调用stop后， 计数为0时，调用底层stop
	//暂未使用
	//int runningNum;


	//驱动自旋锁
	//SpinLock driverLock;

	//缓冲区是否就绪标志
	bool bufferReady;
	//驱动运行标志
	bool driverRuning;
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

public:
	//输入通道信息
	std::vector<ASIOChannelInfo> inputChannels;
	//输出通道信息
	std::vector<ASIOChannelInfo> outputChannels;

private:
	ASIOObject* object;
	ASIOCallbacks* callbacks;

public:

	//void bufferProcess(int bufferIndex, ASIOBool directProcess);

	CLSID driverID;

	int deviceByteSize;    //设备的缓冲区字节数
	int deviceFrameSize;    //设备的帧数
	int perBufferByteSize;  //单次的硬件缓冲字节数
	int perChBufferByteSize; //每个硬件通道的字节数
	int perChLimitByteSize;   //每个通道允许放置的数据量。


	int _iActiveNum;  //激活的输入通道数
 	int _oActiveNum; //激活的输出通道数
	int totalActiveNum;  //总激活的通道

	int inputMask = 0; //输入通道的掩码
	int outputMask = 0; //输出通道的掩码



	std::vector<char> _iTotalBuffers; //总输入缓冲区
	std::vector<char> _oTotalBuffers; //总输出缓冲区


	std::vector<ASIOBuffer2> _cbBuffers; //回调缓冲区


	std::atomic<unsigned> _captureCounter; //输入缓冲区计数器

	std::atomic<unsigned> _renderWritePos; //输入写位置
	std::atomic<unsigned> _renderReadPos;  //输出读位置

	int _haBuffersize; //硬件缓冲区大小

};



