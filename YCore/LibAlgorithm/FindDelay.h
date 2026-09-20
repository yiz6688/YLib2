#pragma once
#include"base_config.hpp"
#include<vector>

class FindDelay
{

public:
	//计算两个信号的相关性
	std::vector<double> correlate(span_ns::span<double> x, span_ns::span<double> y);

	std::vector<double> correlate2(span_ns::span<double> x, span_ns::span<double> y);

	int gcc_phat_delay(span_ns::span<double> x,  span_ns::span<double> y);

};