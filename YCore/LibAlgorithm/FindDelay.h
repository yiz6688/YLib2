#pragma once
#include<vector>

class FindDelay
{





public:
	int corr_delay(std::vector<float> vec1, std::vector<float> vec2);

	int gcc_phat_delay(const std::vector<float>& vec1, const std::vector<float>& vec2);

};