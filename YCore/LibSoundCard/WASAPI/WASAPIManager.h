#pragma once
#include<mmdeviceapi.h>
#include<vector>
#include<string>
#include"../EndPointInfo.h"




class WASAPIManager
{


public:
	static std::vector<EndPointInfo> getEndPoints(EDataFlow eDataFlow, DWORD dwMask = DEVICE_STATE_ACTIVE);



public:
	



};