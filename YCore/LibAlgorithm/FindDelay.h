#pragma once
#include<vector>
#include<span>

class FindDelay
{





public:
	int corr_delay(std::span<double> x, std::span<double> y, int maxLag);

	int gcc_phat_delay(std::span<double> x,  std::span<double> y);

};