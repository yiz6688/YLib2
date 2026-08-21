#pragma once
#include<string>
#include"TResult.h"
#include<memory>
#include"../ICapture.h"
#include"../IRender.h"

struct ASIOInfo
{
	const CLSID clsid;
	const std::string driverName;
};


class ASIODriver;
//ASIO客户端，提供给外部调用的接口，负责与ASIODevice进行交互
class ASIOClient
{
public:
	ASIOClient(CLSID clsid);



public:
	TResult<void> init(CLSID clsid);


public:

	//获取录音器数量
	int getCaptureCount();
	//获取播放器数量
	int getRenderCount();

	std::string getCaptureName(int channel);

	std::string getRenderName(int channel);

	//获取采样率
	int getSampleRate();
	//设置采样率
	TResult<void> setSampleRate(long sampleRate);


public:

	//virtual ~ASIOClient() = 0;

	//virtual ICapture* getCaptureClient(std::initializer_list<int> lst) = 0;

	//virtual IRender* getRenderClient(std::initializer_list<int> lst) = 0;


	//获取录制通道信息
	//获取播放通道信息

public:

	std::unique_ptr<ASIODriver> pDriver;  //驱动的指针。
};