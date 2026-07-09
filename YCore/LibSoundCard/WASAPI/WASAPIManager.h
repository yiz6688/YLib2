#pragma once
#include<mmdeviceapi.h>
#include<vector>
#include<string>
#include"../EndPointInfo.h"
#include"WASAPIRender.h"
#include"WASAPICapture.h"



class WASAPIManager
{


public:
	static std::vector<EndPointInfo> getEndPoints(EDataFlow eDataFlow, DWORD dwMask = DEVICE_STATE_ACTIVE);

	static WASAPIRender* createRender(EndPointInfo&& info, WaveFormat fmt);
	
	static WASAPICapture* createCapture(EndPointInfo&& info, WaveFormat fmt);

public:
	



};