#pragma once
#include<vector>
#include<span>

class FindDelay
{

public:
	//计算两个信号的相关性
	std::vector<double> correlate(std::span<double> x, std::span<double> y);

	int gcc_phat_delay(std::span<double> x,  std::span<double> y);

};