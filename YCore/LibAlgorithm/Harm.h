#pragma once


/// <summary>
/// 谐波的结构体
/// </summary>
struct Harm
{
	
	double baseFreq;  //基准频率

	int order;  //谐波的阶数,根据测试需求，内置最大测试到35或40k，计算的基准频率 
	//阶数不足时，就按照实际的来，比如1000信号，在24k采样率下,计算到20阶即可。

	double* arr; //存储谐波的具体内容


};