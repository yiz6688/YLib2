#pragma once
#include"fftw3.h"
#include<algorithm>
#include<numeric>
#include<vector>
#include<cmath>
//#pragma comment(lib, "fftw3.lib")

using std::vector;

class Harmonic
{

public:
	static void doFFT(std::vector<double> data,  int nfft,  int sampleRate)
	{
		int dataLen = data.size();

		int _nfft = 4096;
		int nout = nfft / 2 + 1;  //fft输出
		vector<double> win; // = getWindiow(nfft, blackman);  //获取窗函数
		int noverlap = 0.5 * nfft;  //重叠长度

		int nFrame = (dataLen - noverlap) / (nfft - noverlap); //数据帧长度

		fftw_complex* out;
		vector<double> frame(nfft);
		vector<double> result(nout);   //存储结果
		vector<double> freqs(nout);    //频率
		std::fill_n(result.begin(), nout, 0.0);  //清0
		out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * nout);
		fftw_plan p = fftw_plan_dft_r2c_1d(nfft, frame.data(), out, FFTW_ESTIMATE);


		int hope_size = 0;  //重叠大小

		for (int i = 0; i < nFrame; i++)
		{
			auto iter = data.begin() + i * (nfft - noverlap); //帧起始位置
			for (int j = 0; j < nfft; j++)   //加窗
			{
				frame[j] = *(iter + j) * win[j];
			}

			fftw_execute(p);

			for (int j = 0; j < nout; j++)
			{
				result[j] += sqrt(out[j][0] * out[j][0] + out[j][1] * out[j][1]) / nfft;
				//result[j] *= 4.762 / (nFrame);   //2*2.381
			}
		}

		auto amp = std::accumulate(win.begin(), win.end(), 0.0);  //窗的和
		auto k = amp * nFrame; // 除以的系数
		for (int i = 0; i < nout; i++)  //0和奈奎斯特不用修正，中间的修正
		{
			result[i] /= k;
		}
	}


	//计算扫频信号拆解出来的每一个正弦信号, 使用全fft分析，不分帧
	static void doFFT2(std::vector<double> data, int sampleRate)
	{
		int dataLen = data.size();

		int nfft = dataLen;
		int nout = nfft / 2 + 1;  //fft输出

		vector<double> win;

		fftw_complex* out;
		vector<double> frame(nfft);
		vector<double> result(nout);   //存储结果
		vector<double> freqs(nout);    //频率
		std::fill_n(result.begin(), nout, 0.0);  //清0
		out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * nout);
		fftw_plan p = fftw_plan_dft_r2c_1d(nfft, frame.data(), out, FFTW_ESTIMATE);
		
		for (int j = 0; j < nfft; j++)   //加窗
		{
			frame[j] = data[j]* win[j];
		}

		fftw_execute(p);

		//每个里面都是power
		for (int j = 0; j < nout; j++)
		{
			result[j] += out[j][0] * out[j][0] + out[j][1] * out[j][1] / nfft;
			//result[j] *= 4.762 / (nFrame);   //2*2.381
		}


		//这里计算的是PSD  这是功率信号，计算功率谱密度， 再除以采样率
		auto amp = std::accumulate(win.begin(), win.end(), 0.0, [](auto a, auto b) {
			return a * a + b * b;
			});  //窗的和
		auto k = amp ; // 除以的系数
		for (int i = 0; i < nout; i++)  //0和奈奎斯特不用修正，中间的修正
		{
			result[i] /= k;
		}


	}



	static vector<double> getFreq(int nfft, int sampleRate)
	{
		int nout = nfft / 2 + 1;

		vector<double> freq;
		freq.resize(nout);
		
		for (int i = 0; i < nout; i++)
		{
			freq[i] = i * sampleRate / nfft;
		}

		return freq;
	}


	//periodogram周期图

	//获取PSD
	static vector<double> getPSD(std::vector<double> data, int sampleRate)
	{
		int dataLen = data.size();

		int nfft = dataLen;
		int nout = nfft / 2 + 1;  //fft输出

		vector<double> win;

		fftw_complex* out;
		vector<double> frame(nfft);
		vector<double> result(nout);   //存储结果
		vector<double> freqs(nout);    //频率
		std::fill_n(result.begin(), nout, 0.0);  //清0
		out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * nout);
		fftw_plan p = fftw_plan_dft_r2c_1d(nfft, frame.data(), out, FFTW_ESTIMATE);

		for (int j = 0; j < nfft; j++)   //加窗
		{
			frame[j] = data[j] * win[j];
		}

		fftw_execute(p);

		//每个里面都是power
		for (int j = 0; j < nout; j++)
		{
			result[j] += (out[j][0] * out[j][0] + out[j][1] * out[j][1]);
			//result[j] *= 4.762 / (nFrame);   //2*2.381
		}



		//fft结果 = sqrt(power) /nfft.   能量为power/nfft^2
		//窗的能量恢复系数为 窗的能量和/nfft.    恢复计算psd第一步为  power/(窗的能量和*nfft)
		//频率分辨率为 sampleRate / nfft.      psd初步结算结果除以频率分辨率。      最终计算为  power*2/(窗的能量和*采样率)
		//psd表示单位频率的功率， fft的计算结果是每个频率带的总功率， 除以单位频率就是 v^2/hz   就能计算出来单位频率内的功率


		//这里计算的是PSD  这是功率信号，计算功率谱密度， 再除以采样率
		auto amp = std::accumulate(win.begin(), win.end(), 0.0, [](auto a, auto b) {
			return a + b * b;
			});  //窗的和
		auto k = amp ; // 除以的系数
		for (int i = 0; i < nout; i++)  //0和奈奎斯特不用修正，中间的修正
		{
			result[i] /= k;
		}

		return result;
	}



	//计算窗函数的enbw
	static double enbw(vector<double> win, int sampleRate)
	{

		auto v1 = std::accumulate(win.begin(), win.end(), 0.0, [](double a, double b) {
			return a * a + b * b;
			}); //窗的平方和

		auto v2 = std::accumulate(win.begin(), win.end(), 0.0);//窗的和
		//bw = (rms/mean) ^ 2;
		auto enbw = v1 * win.size() / (v2 * v2);

		auto v3 = enbw * (sampleRate / win.size());

		//这里的公式可以简化 相约

		return v3;  

	}



	static std::array<int,3> getToneFromPSD(vector<double> pxx, vector<double> freqs, double baseFreq, int order)
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

	static double getPowerFreq(vector<double> pxx, vector<double> freqs, std::array<int, 3>indexs)
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
	static void computeHarm(vector<double>pxx, vector<double> freqs, float rsolu, float baseFreq,  int order )
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





};


