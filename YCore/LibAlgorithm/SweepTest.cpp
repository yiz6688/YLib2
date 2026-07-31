
#define _USE_MATH_DEFINES
#include<algorithm>
#include"SweepTest.h"
#include<stdexcept>
#include<string>
#include<vector>
#include<cmath>
#include<print>
#include<fstream>
#include"FindDelay.h"
#include"Harmonic.h"
#include"Window.h"
#include"FFT.h"

using namespace std;

//优先数系 R80,对应 1/24倍频程
std::vector<double> R80_1 = { 1 ,1.03 ,1.06 ,1.09 ,1.12 ,1.15 ,1.18 ,1.22 ,1.25 ,1.28 ,1.32 ,1.36 ,1.4 ,1.45 ,1.5 ,1.55 ,1.6 ,1.65 ,1.7 ,1.75 ,1.8 ,1.85 ,1.9 ,1.95 ,
		2 ,2.06 ,2.12 ,2.18 ,2.24 ,2.3 ,2.36 ,2.43 ,2.5 ,2.58 ,2.65 ,2.72 ,2.8 ,2.9 ,3 ,3.07 ,3.15 ,3.25 ,3.35 ,3.45 ,3.55 ,3.65 ,3.75 ,3.87 ,
		4 ,4.12 ,4.25 ,4.37 ,4.5 ,4.62 ,4.75 ,4.87 ,5 ,5.15 ,5.3 ,5.45 ,5.6 ,5.8 ,6 ,6.15 ,6.3 ,6.5 ,6.7 ,6.9 ,7.1 ,7.3 ,7.5 ,7.75 ,
		8 ,8.25 ,8.5 ,8.75 ,9 ,9.25 ,9.5 ,9.75 };


std::vector<double> R80_2 = {1,1.03,1.06,1.09,1.12,1.15,1.18,1.22,1.25,1.28,1.32,1.36,1.4,1.45,1.5,1.55,1.6,
				1.65,1.7,1.75,1.8,1.85,1.9,1.95,2,2.06,2.12,2.18,2.24,2.3,2.36,2.43,2.5,2.58,
				2.65,2.72,2.8,2.9,3,3.07,3.15,3.25,3.35,3.45,3.55,3.65,3.75,3.87,4,4.12,4.25,
				4.37,4.5,4.62,4.75,4.87,5,5.15,5.3,5.45,5.6,5.8,6,6.15,6.3,6.5,6.7,6.9,7.1,7.3,
				7.5,7.75,8,8.25,8.5,8.75,9,9.25,9.5,9.75,10,10.3,10.6,10.9,11.2,11.5,11.8,12.2,
				12.5,12.8,13.2,13.6,14,14.5,15,15.5,16,16.5,17,17.5,18,18.5,19,19.5,20,20.6,21.2,
				21.8,22.4,23,23.6,24.3,25,25.8,26.5,27.2,28,29,30,30.7,31.5,32.5,33.5,34.5,35.5,
				36.5,37.5,38.7,40,41.2,42.5,43.7,45,46.2,47.5,48.7,50,51.5,53,54.5,56,58,60,61.5,
				63,65,67,69,71,73,75,77.5,80,82.5,85,87.5,90,92.5,95,97.5,100,103,106,109,112,115,
				118,122,125,128,132,136,140,145,150,155,160,165,170,175,180,185,190,195,200,206,212,
				218,224,230,236,243,250,258,265,272,280,290,300,307,315,325,335,345,355,365,375,387,
				400,412,425,437,450,462,475,487,500,515,530,545,560,580,600,615,630,650,670,690,710,
				730,750,775,800,825,850,875,900,925,950,975,1000,1030,1060,1090,1120,1150,1180,1220,
				1250,1280,1320,1360,1400,1450,1500,1550,1600,1650,1700,1750,1800,1850,1900,1950,2000,
				2060,2120,2180,2240,2300,2360,2430,2500,2580,2650,2720,2800,2900,3000,3070,3150,3250,
				3350,3450,3550,3650,3750,3870,4000,4120,4250,4370,4500,4620,4750,4870,5000,5150,5300,
				5450,5600,5800,6000,6150,6300,6500,6700,6900,7100,7300,7500,7750,8000,8250,8500,8750,
				9000,9250,9500,9750,10000,10300,10600,10900,11200,11500,11800,12200,12500,12800,13200,
				13600,14000,14500,15000,15500,16000,16500,17000,17500,18000,18500,19000,19500,20000,
				20600,21200,21800,22400,23000,23600,24300,25000,25800,26500,27200,28000,29000,30000,
				30700,31500,32500,33500,34500,35500,36500,37500,38700,40000,41200,42500,43700,45000,
				46200,47500,48700,50000,51500,53000,54500,56000,58000,60000,61500,63000,65000,67000,
				69000,71000,73000,75000,77500,80000,82500,85000,87500,90000,92500,95000,97500,100000};


StepSweep::StepSweep()
{
}

StepSweep::~StepSweep()
{
}

vector<double> StepSweep::GenerateSweepFreq(double startHz, double stopHz, Octave oct)
{
	auto& R80 = R80_1;
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

std::vector<double> StepSweep::GenerateSweepFreq2(double startHz, double stopHz, Octave oct)
{
	auto& R80 = R80_2;

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

	auto iter = std::lower_bound(R80.begin(), R80.end(), static_cast<float>(start));
	int offset = std::distance(R80.begin(), iter);

	double freq = start;

	int size = R80.size();
	vector<double> vec;

	while (freq < stop)
	{
		freq = R80[offset];
		vec.push_back(freq);
		offset += interval;
	}

	//倒序的
	if (flag)
	{
		return vector<double>(vec.rbegin(), vec.rend());
		
	}
	return vec;
}

void exportSource(AudioSource* src, string filepath)
{
	ofstream ofs(filepath);
	ofs<<"sampleRate: "<<src->sampleRate<<"\n";
	ofs<<"totalFrq: "<<src->totalFrq<<"\n";
	ofs<<"totalSample: "<<src->totalSample<<"\n";
	ofs<<"totalTime: "<<src->totalTime<<"\n";

	for(int i=0; i< src->totalFrq; i++)
	{
		auto& info = src->infos[i];
		ofs<<i<<" "<<info.freq<<", "<<info.cycle<<", "<<info.Q<<", "
		<<info.durationStart<<", "<<info.duration<<", "<<info.sampleStart<<", "<<info.sampleNum<<"\n";
	}

	ofs.close();
}


AudioSource StepSweep::GenerateSweepWave(double startHz, double stopHz, int minCycle, int minDuration, Octave oct, int type)
{
	//最小持续时间单位是毫秒
	int SampleRate = 48000;
	vector<double> freqlst = GenerateSweepFreq(startHz, stopHz, oct);
	vector<SweepInfo> infos;


	int totalSample = 0;
	double totalTime = 0;

	int sampleStart = 0;
	double durationStart = 0;
	for (double freq : freqlst)
	{
		double durationCycle = minDuration * freq / 1000.0;  //计算最小持续时间下的周期数, 时间/周期= 周期数

		int nCycle = 0;
		if (minCycle < durationCycle)
		{
			nCycle = static_cast<int>(durationCycle);
			if(type == 0)
			{
				if(durationCycle - nCycle > 0.5)
				{
					nCycle++;
				}
			}else
			{
				if(durationCycle - nCycle > 0)
				{
					nCycle++;
				}	
			}

			if (nCycle > minCycle * 400) { nCycle = minCycle * 400; };
		}

		nCycle = max<int>(nCycle, minCycle);   //取较大的

		SweepInfo info;
		info.freq = freq;
		info.cycle = nCycle;
		
		double sampleNum = SampleRate * info.cycle / freq;
		if(type == 0)
		{
			info.sampleNum = static_cast<int>(round(sampleNum));  //采样点数,4舍5入
		}else
		{
			int k = static_cast<int>(sampleNum);
			if(sampleNum - k > 0.5)
			{
				k++;
			}else if(sampleNum - k == 0.5)
			{
				if(k%2!= 0)
				{
					k++;
				}
			}
			info.sampleNum = k;
		}

		info.duration = info.sampleNum * 1.0 / SampleRate;   //每个频点持续的时间
		
		info.durationStart = durationStart;
		info. sampleStart = sampleStart;

		sampleStart += info.sampleNum;
		durationStart += info.duration;

		totalSample += info.sampleNum;
		totalTime += info.duration;

		infos.push_back(info);
	}

	int ExtenLen = 2048;

	vector<double> wavedata;

	double Q = 0;
	for (auto& info : infos)
	{
		info.Q = Q * 360;
		if(type == 0)
		{
			double realCycle = 1.0 * info.sampleNum * info.freq / SampleRate + Q;
			
			auto error = abs(realCycle - info.cycle);

			if(error > 0.000001)
			{
				Q = realCycle - info.cycle;
			}else
			{
				Q = 0;
			}
		}

	}

	AudioSource src;
	src.sampleRate = SampleRate;
	src.totalFrq = infos.size();
	src.totalTime = totalTime;
	src.totalSample = totalSample;
	src.infos = infos;

	//exportSource(&src, "D:\\sc.txt");

	return src;
}

//创建信号
std::vector<double> StepSweep::createWave(AudioSource *src, int type)
{
	std::vector<double> samples;
	int postLen = 0;
	int fakeLen = 2048;
	int offset = 0;
	if(type == 0)
	{
		postLen = src->sampleRate * 10 / 1000;
		samples.reserve(src->totalSample + postLen + fakeLen + 1);
		offset = 1;
		samples.push_back(0);
	}else
	{
		samples.reserve(src->totalSample);
	}

	for(auto& info: src->infos)
	{
		for(int i=offset; i< info.sampleNum + offset; i++)
		{
			double t =  i * info.freq / src->sampleRate   + info.Q;
			double value = sin(2 * M_PI * t);
			samples.push_back(value);
		}
	}

	if(type == 0)
	{
		auto win = this->tukeyWin(postLen * 2, 0.986);
		auto last = src->infos.back();
		for(int i=0; i<postLen; i++)
		{
			double t = (i + offset) * last.freq / src->sampleRate   + last.Q;
			double value = sin(2*M_PI *t) * win[i];
			samples.push_back(value);
		}


		for(int i=0; i<fakeLen;i++)
		{
			samples.push_back(0);
		}

	}


    return samples;
}

std::vector<double> StepSweep::tukeyWin(int n, double ratio)
{
	std::vector<double> data(n);
	auto per = ratio / 2;
	auto low = per*(n-1) + 1;
	auto hig = n - low + 1;

	for(int i=0; i<n;i++)
	{
		double t = i * 1.0 / n;
		if(i < low)
		{
			data[i] = (1 + cos(M_PI / per *(t-per))) / 2;
		}else if( i> hig)
		{
			data[i] = (1 + cos(M_PI / per*(t-1+per))) / 2;
		}else
		{
			data[i] = 1;
		}
	}



    return std::vector<double>();
}

void StepSweep::sweepTest(vector<double>& wav, double startHz, double stopHz, int minCycle, int minDuration, Octave oct, int type)
{
	int sampleRate = 48000;
	//这里是不是要核对一下 假如录音文件长度不够，只有半截，或者其他场景的相关问题。
	FindDelay fd;

	auto src = this->GenerateSweepWave(startHz, stopHz, minCycle, minDuration, oct, type);
	auto stdWave = this->createWave(&src, type);
	//查找音频文件起始位置
	int findDelay = fd.gcc_phat_delay(wav, stdWave);

	std::println("延迟: {}", findDelay);


	float rate = 0.05;

	std::vector<int> sample2(src.totalFrq);  //缓存cut后的点。
	int cutCycle = 0;
	int maxLen = 0;

	for(int i=0; i< src.totalFrq; i++)
	{
		auto& info = src.infos[i];  
		cutCycle = info.cycle - 1;

		sample2[i] =  static_cast<int>(info.sampleNum * cutCycle / info.cycle);
		if(maxLen < sample2[i])
		{
			maxLen = sample2[i];
		}
	}

	
	std::vector<double> vec(maxLen); //计算FFT的输入空间。
	int offset = findDelay;

	for(int i= 0;i<src.totalFrq; i++)
	{
		
		int fftLen = sample2[i];
		CosineWindow window;
		auto win = window.getWindiow(fftLen, CosineWindow::Blackman_Harris); //使用黑人哈里斯窗
		
		auto& info = src.infos[i];

		for(int i=0; i< fftLen; i++)
		{
			vec[i] = wav[findDelay + info.sampleStart] * win[i];  //加窗
		}

		FFT fft(fftLen);
		fft.fft(vec); //进行fft;

		auto powerWin = std::accumulate(win.begin(), win.end(), 0.0, [](auto a, auto b){
			return a*a + b*b;
		});  //窗的能量。



	}









	int size = this->sweepInfos.size();
	this->harms.resize(size); //设置这么多内容

	

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


array<double, 5> blackmanharris{ 0.35875, 0.48829, 0.14128, 0.01168};
array<double, 5> blackman{ 0.42, 0.50, 0.08 };


vector<double> getWindiow(int N, array<double,5>& coef)
{
	//ofstream ofs("D:\\123.txt");
	vector<double> win(N);
	double fact = 1.0*2*M_PI / (N-1);
	for (int i = 0; i < N; i++)
	{
		win[i] = coef[0] - coef[1] * cos(fact * i) + coef[2] * cos(2 * fact * i) 
			- coef[3] * cos(3 * fact * i) + coef[4] * cos(4 * fact * i);
		//ofs << win[i] << endl;
	}

	return win;
}



bool NoistTest(std::vector<double>& wavdata, int sampleRate)
{
	int spkLen = wavdata.size();

	int nfft = 4096;
	int nout = nfft / 2 + 1;  //fft输出
	vector<double> win = getWindiow(nfft, blackman);
	int noverlap = 0.5 * nfft;  //重叠长度

	int nFrame = (spkLen - noverlap)/(nfft - noverlap) ; //数据帧长度

	fftw_complex* out;
	vector<double> frame(nfft);
	vector<double> result(nout);   //存储结果
	vector<double> freqs(nout);    //频率
	std::fill_n(result.begin(),nout, 0.0);  //清0
	out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * nout);
	fftw_plan p= fftw_plan_dft_r2c_1d(nfft, frame.data(), out, FFTW_ESTIMATE);

	for (int i = 0; i < nFrame; i++)
	{
		auto iter =wavdata.begin() + i * (nfft - noverlap); //帧起始位置
		for (int j = 0; j < nfft; j++)   //加窗
		{
			frame[j] = *(iter + j) * win[j];
		}

		fftw_execute(p);

		for (int j = 0; j < nout; j++)
		{
			result[j]+= sqrt(out[j][0] * out[j][0] + out[j][1] * out[j][1])/nfft;
			result[j] *= 4.762/(nFrame);   //2*2.381
		}
	}

	for (int i = 0; i < nout; i++)
	{
		freqs[i] = sampleRate * 1.0*i / nfft;
	}

	std::vector<double> y(nout);
	
	for (int i = 0; i < nout; i++)
	{
		y[i] = 20 * log10(result[i]);
	}
	

	fftw_destroy_plan(p);
	fftw_cleanup();
	fftw_free(out);


	return true;
}
