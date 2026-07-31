#pragma once
#include<vector>

enum class Octave
{
	OCT3,  // 三分之一倍频程
	OCT6,  //六分之一倍频程
	OCT12,  //十二分之一倍频程
	OCT24   //二十四分之一倍频程
};

enum class SweepType
{
	SC,  //soundcard类型
	RS	//瑞森类型
};

struct SweepInfo
{
	//频率
	double freq;
	//周期数
	int cycle;
	//起始时间
	double durationStart;
	//持续时间
	double duration;
	//采样点起始时间
	int sampleStart;
	//采样点数
	int sampleNum;
	//初始相位
	double Q;


};


struct AudioSource
{
	double sampleRate;
	int totalFrq;
	int totalSample;
	double totalTime;

	double startHz;
	double stopHz;
	int minCycle;
	int minDuration;
	Octave octave;


	std::vector<SweepInfo> infos;
};


struct Harm
{

	double baseFreq;  //基准频率

	int order;  //谐波的阶数,根据测试需求，内置最大测试到35或40k，计算的基准频率 
	//阶数不足时，就按照实际的来，比如1000信号，在24k采样率下,计算到20阶即可。

	std::vector<double> values;  //谐波的值

};


struct AudioResult
{

	std::vector<Harm> harms;
};



class StepSweep
{
public:
	StepSweep();
	~StepSweep();
	std::vector<double> GenerateSweepFreq(double startHz, double stopHz, Octave oct);
	std::vector<double> GenerateSweepFreq2(double startHz, double stopHz, Octave oct);

	AudioSource GenerateSweepWave(double startHz, double stopHz, int minCycle, int minDuration, Octave oct, int type);

	std::vector<double> createWave(AudioSource* src, int type);


	std::vector<double> tukeyWin(int n, double ratio);

	void sweepTest(std::vector<double>& data, double startHz, double stopHz, int minCycle, int minDuration, Octave oct, int type);

private:
	std::vector<SweepInfo> sweepInfos;	//频率扫描信息列表
	std::vector<float> waveData;		//生成的原始波形数据
	float sampleRate = 48000.0f;		//采样率

	std::vector<Harm> harms;   //谐波分量长度等同于sweepInfos

	double startFreq;

	double stopFreq;

	Octave oct;

};