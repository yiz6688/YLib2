
#define _USE_MATH_DEFINES
#include<algorithm>
#include"SweepTest.h"
#include<stdexcept>
#include<string>
#include<vector>
#include<cmath>
#include<print>

#include"FindDelay.h"
#include"Harmonic.h"

using namespace std;

//优先数系 R80,对应 1/24倍频程
std::vector<float> R80 = { 1 ,1.03 ,1.06 ,1.09 ,1.12 ,1.15 ,1.18 ,1.22 ,1.25 ,1.28 ,1.32 ,1.36 ,1.4 ,1.45 ,1.5 ,1.55 ,1.6 ,1.65 ,1.7 ,1.75 ,1.8 ,1.85 ,1.9 ,1.95 ,
		2 ,2.06 ,2.12 ,2.18 ,2.24 ,2.3 ,2.36 ,2.43 ,2.5 ,2.58 ,2.65 ,2.72 ,2.8 ,2.9 ,3 ,3.07 ,3.15 ,3.25 ,3.35 ,3.45 ,3.55 ,3.65 ,3.75 ,3.87 ,
		4 ,4.12 ,4.25 ,4.37 ,4.5 ,4.62 ,4.75 ,4.87 ,5 ,5.15 ,5.3 ,5.45 ,5.6 ,5.8 ,6 ,6.15 ,6.3 ,6.5 ,6.7 ,6.9 ,7.1 ,7.3 ,7.5 ,7.75 ,
		8 ,8.25 ,8.5 ,8.75 ,9 ,9.25 ,9.5 ,9.75 };


StepSweep::StepSweep()
{
}

StepSweep::~StepSweep()
{
}

vector<double> StepSweep::GenerateSweepFreq(double startHz, double stopHz, Octave oct)
{
	if (startHz <= 0 || stopHz <= 0)
	{
		throw std::invalid_argument("startHz or stopHz must bigger than zero");

	}

	int interval = 1;  //相比R80的跳跃间隔
	if (oct == Octave::OCT3)
	{
		interval = 8;
	}
	else if (oct == Octave::OCT6)
	{
		interval = 4;
	}
	else if (oct == Octave::OCT12)
	{
		interval = 2;
	}
	else
	{
		interval = 1;
	}

	bool flag = false;
	double start = startHz, stop = stopHz;
	if (startHz > stopHz)
	{
		start = stopHz;
		stop = startHz;
		flag = true;
	}
	int exp = int(log10(start));  //起始频率计算映射后的值
	double rate = pow(10, exp);
	double freq = start / rate;   //起始频率
	auto iter = std::lower_bound(R80.begin(), R80.end(), static_cast<float>(freq));
	int offset = std::distance(R80.begin(), iter);

	int k = offset % interval;
	if (k != 0)
	{
		offset -= k;
	}

	int size = R80.size();
	vector<double> vec;

	while (freq < stop)
	{
		freq = R80[offset] * rate;
		vec.push_back(freq);
		offset += interval;
		if (offset >= size)
		{
			offset = 0;
			exp++;
			rate = static_cast<int>(pow(10, exp));
		}
	}

	//倒序的
	if (flag)
	{
		return vector<double>(vec.rbegin(), vec.rend());
		
	}
	return vec;
}

/// <summary>
///  需要的参数，根据最小持续时间计算出最大持续周期数，和最小周期相比，取大的
///  计算出当前频率需要的点数
/// 计算出当前频率持续的时间
/// </summary>
/// <param name="startHz"></param>
/// <param name="stopHz"></param>
/// <param name="minCycle"></param>
/// <param name="minDuration"></param>
/// <param name="oct"></param>
/// 
void StepSweep::GenerateSweepWave(double startHz, double stopHz, int minCycle, int minDuration, Octave oct)
{
	//最小持续时间单位是毫秒
	int SampleRate = 48000;
	vector<double> freqlst = GenerateSweepFreq(startHz, stopHz, oct);
	vector<SweepInfo> infos;

	int SampleStart = 0;
	double DurationStart = 0;
	for (double freq : freqlst)
	{
		double Cycle = minDuration * freq / 1000;  //计算最小持续时间下的周期数, 时间/周期= 周期数

		int nCycle = 0;
		if (minCycle < Cycle)
		{
			nCycle = static_cast<int>(round(Cycle));
			// nCycle = static_cast<int>(Cycle);   //每个频点循环的周期数,四舍五入
			// if (Cycle - nCycle > 0.5)
			// {
			// 	nCycle++;
			// }
			if (nCycle > minCycle * 400) { nCycle = minCycle * 400; };
		}

		nCycle = max<int>(nCycle, minCycle);   //取较大的

		SweepInfo info;
		info.freq = freq;
		info.cycle = nCycle;
		info.sampleNum = static_cast<int>(info.cycle * 1.0 * SampleRate / freq + 0.5);  //采样点数
		info.duration = info.sampleNum * 1.0 / SampleRate;   //每个频点持续的时间

		string Msg = "Freq: " + to_string(info.freq) + " Cycle: " + to_string(info.cycle) + " Sample: " + to_string(info.sampleNum)
			+ " Duration: " + to_string(info.duration) + " DurationStart: " + to_string(DurationStart) + " SampleStart: " + to_string(SampleStart);
		//cout << Msg << endl;
		//logger.Debug(Msg);
		SampleStart += info.sampleNum;
		DurationStart += info.duration;
		infos.push_back(info);
	}
	string Msg = " ************* DurationStart: " + to_string(DurationStart) + " SampleStart: " + to_string(SampleStart);
	//logger.Debug(Msg);
	int ExtenLen = 2048;
	Msg = " ************* TotalDuration: " + to_string(DurationStart) + " TotalSample: " + to_string(SampleStart + ExtenLen + 1);
	//logger.Debug(Msg);

	vector<double> wavedata;
	double Q = 0;

	for (auto const& info : infos)
	{
		double realCycle = 1.0 * info.sampleNum * info.freq / SampleRate + Q;

		Q = realCycle - info.cycle;
	}



}

#include<array>


std::array<int,3> getToneFromPSD(vector<double> pxx, vector<double> freqs, double baseFreq, int order)
{
	//这里计算出来的是目标频率，基频乘以谐波阶数
	double freq = baseFreq * order;

	int maxToneIndex;  //最大值所在的位置
	double maxTone;   //最大的谐波

	if (order == 1)
	{

		auto iter = std::max(pxx.begin(), pxx.end());
		maxToneIndex = std::distance(pxx.begin(), iter);
		maxTone = *iter;
	}
	else if (freq >= freqs.front() && freq <= freqs.back())
	{
		//查找最近的频点
		auto iter = std::lower_bound(freqs.begin(), freqs.end(), freq);
		auto iter2 = iter - 1;
		if (*iter - freq > freq - *iter)
		{
			iter = iter2;  //说明前一个点更接近freq
		}
		
		int index = std::distance(freqs.begin(), iter);
		//按照左右分离 计算边界
		int left = std::max(0, index - 1);
		int right = std::min(index + 1, int(freqs.size() - 1));

		iter = std::max(freqs.begin() + left, freqs.begin() + right);
		//在分离的范围内获取最大值，刷新标志位
		maxToneIndex = std::distance(freqs.begin(), iter);
		maxTone = *iter;
	}

	int left = maxToneIndex - 1;
	int right = maxToneIndex + 1;

	//left 计算左侧第一个开始变大的点，  right计算右侧第一个开始变大的点
	while (left >= 0 && pxx[left - 1] < pxx[left])
	{
		left -= 1;
	}

	while (right < pxx.size() && pxx[right + 1] < pxx[right])
	{
		right += 1;
	}


	return std::array<int, 3>{left, maxToneIndex, right};

}

double getPowerFreq(vector<double> pxx, vector<double> freqs, std::array<int, 3>indexs)
{
	int left = indexs[0];
	int tonInex = indexs[1];
	int right = indexs[2];

	double v1 = 0;
	double v2 = 0;

	double calcFreq = 0.0; //计算出来的频率

	if (left < right)
	{
		for (int i = left; i < right; i++)
		{
			v1 += pxx[i] * freqs[i];
			v2 += pxx[i];
		}

		calcFreq = v1 / v2;  //计算出来的基准频率  
	}


	return calcFreq;
}



//计算谐波
void computeHarm(vector<double>pxx, vector<double> freqs, float rsolu, float baseFreq,  int order )
{

	double targetFreq = baseFreq * order; //目标频率

	if (order == 1)
	{
		pxx[0] *= 2;

		std::array<int, 3> res = getToneFromPSD(pxx, freqs, baseFreq, 0);
		int left = res[0] + 1;
		int toneIndex = res[1];
		int right = res[2] - 1;

		for (int i = left; i < right; i++)
		{
			pxx[i] = 0;  //对基频清0
		}


		res = getToneFromPSD(pxx, freqs, baseFreq, 1);
		 left = res[0] + 1;
		toneIndex = res[1];
		right = res[2] - 1;

		double freq = getPowerFreq(pxx, freqs, res);  //获取频率

		for (int i = left; i < right; i++)
		{
			pxx[i] = 0;
		}
	}
	else
	{
		auto res = getToneFromPSD(pxx, freqs, baseFreq, 1);
		int left = res[0] + 1;
		int toneIndex = res[1];
		int right = res[2] - 1;

		double freq = getPowerFreq(pxx, freqs, res);  //获取频率

	}



}






void StepSweep::sweepTest(vector<float> data)
{
	//这里是不是要核对一下 假如录音文件长度不够，只有半截，或者其他场景的相关问题。
	FindDelay fd;
	//查找音频文件起始位置
	//int findDelay = fd.corr_delay(data, this->waveData);
	int findDelay = 0;
	int sampleRate = 48000;

	float rate = 0.05;

	int size = this->sweepInfos.size();
	this->harms.resize(size); //设置这么多内容

	int offset = findDelay;

	for (int i = 0; i < size; i++)
	{
		SweepInfo info = this->sweepInfos[i]; //获取设置的频率值
		
		//vector<float> pcm(data.begin() + offset, data.begin() + offset + info.sampleNum);
		vector<double> pcm;
		vector<double> win;

		auto freqs = Harmonic::getFreq(info.sampleNum, sampleRate);

		//获取窗函数的rbw，后面使用
		auto rbw = Harmonic::enbw(win, sampleRate);

		//获取psd，后续的计算估计都使用信号的能量进行
		auto psd = Harmonic::getPSD(pcm, sampleRate);

	


		int harmOrder = 40;

		int maxOrder = sampleRate / (2 * info.freq);

		if (harmOrder > maxOrder)
		{
			harmOrder = maxOrder;
		}


		auto& harm = this->harms[i];

		harm.baseFreq = info.freq;
		harm.order = harmOrder;
		harm.values.resize(harmOrder);

		//计算指定阶数的谐波
		for (int i = 0; i < harmOrder; i++)
		{



		}

	}





}


