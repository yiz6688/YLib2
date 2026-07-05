#pragma once
#include<cmath>


//ansi s1.11-2004标准  这个标准用来计算粉噪的频响
class NoiseTest
{



public:
	void noiseValue()
	{
		float startHz;
		float stopHz;
		int oct; //倍频程
		float sampleRate;  //采样率

		float G = pow(10, 0.3);  //G10 = 10^(3/10),  G2 = 2;

		float fx = 0.0f;  //fx是上一个频率
		int x = 0;        //x表示步数，这个可以由频率范围计算出来 ，比如20-20k 1/12倍频程 121个点

		float fm = pow(G, (2 * x - 59) / (2 * oct)) * fx;   //滤波器的中心频率
		float f1 = pow(G, -1 / (2 * oct)) * fm;   //带通滤波器的下边界
		float f2 = pow(G, 1 / (2 * oct)) * fm;   //带通滤波器的上边界

		//相当于计算除了一个  pow(G, 1/(2*oct)),  下边界是中心频率除以倍率， 上边界是中心频率乘以倍率




	}


};