#pragma once
#include<span>
#include<numeric>
#include<cmath>

class SignalProc
{




public:
	//计算rms, detrend 是否去趋势
	static double getRMS(std::span<double> data, bool detrend)
	{
		auto size = data.size();
		double power = 0.0;
		if (detrend)
		{
			auto sum = std::accumulate(data.begin(), data.end(), 0.0f); //计算直流分量
			auto dc = sum / size;
			power = std::accumulate(data.begin(), data.end(), 0.0f,
				[dc](auto a, auto b)
				{
					b -= dc;
					return a + b * b;
				});
		}
		else
		{
			power = std::accumulate(data.begin(), data.end(), 0.0f,
				[](auto a, auto b){return a + b * b;});
		}

		auto rms = std::sqrt(power / size); //计算均方差 rms

		return rms;
	}



	




};