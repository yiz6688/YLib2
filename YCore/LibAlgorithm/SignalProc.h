#pragma once
#include<vector>
#include<numeric>
#include<cmath>

class SignalProc
{




public:
	//计算rms, detrend 是否去趋势
	static double getRMS(std::vector<double> data, int skip_beg, int skip_end, bool detrend)
	{
		auto size = data.size() - skip_beg - skip_end;
		double power = 0.0;
		if (detrend)
		{
			auto sum = std::accumulate(data.begin() + skip_beg, data.end() - skip_end, 0.0f); //计算直流分量
			auto dc = sum / size;
			power = std::accumulate(data.begin() + skip_beg, data.end() - skip_end, 0.0f,
				[dc](auto a, auto b)
				{
					b -= dc;
					return a + b * b;
				});
		}
		else
		{
			power = std::accumulate(data.begin() + skip_beg, data.end() - skip_end, 0.0f,
				[](auto a, auto b){return a + b * b;});
		}

		auto rms = std::sqrt(power / size); //计算均方差 rms

		return rms;
	}


};