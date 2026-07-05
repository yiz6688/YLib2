#pragma once
#include<string>
#include"../ICapture.h"
#include"../IRender.h"


//ASIO客户端，提供给外部调用的接口，负责与ASIODevice进行交互
class ASIOClient
{
public:

	virtual ~ASIOClient() = 0;

	virtual ICapture* getCaptureClient(std::initializer_list<int> lst) = 0;

	virtual IRender* getRenderClient(std::initializer_list<int> lst) = 0;


	//获取录制通道信息
	//获取播放通道信息

public:





};