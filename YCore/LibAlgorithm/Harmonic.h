#pragma once
#include"fftw3.h"
#include<algorithm>
#include<numeric>
#include<vector>
#include<cmath>
#include<map>
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

	static void periodogram(std::vector<double>& fftValue, int size, double powerWin, int sampleRate, std::vector<double>& pxx)
	{
		auto len = size / 2;
		pxx[0] = fftValue[0] / powerWin / sampleRate;
		pxx[len] = fftValue[len] / powerWin / sampleRate;
		for(int i=1; i<len; i++)
		{
			pxx[i] = 2 * fftValue[i] / powerWin / sampleRate;
		}
	}


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

		auto power = std::accumulate(win.begin(), win.end(), 0.0, [](double a, double b) {
			return a + b * b;
			}); //窗的平方和

		auto sum = std::accumulate(win.begin(), win.end(), 0.0);//窗的和

		auto size = win.size();

		auto rms = sqrt(power / size);
		auto mean = sum / size;
		auto bw = pow(rms / mean, 2);	//bw = (rms/mean) ^ 2;
		
		auto bw2 = power * size / (sum * sum);


		auto rbw = bw * sampleRate / size;
		auto rbw2 = bw2 * sampleRate / size;
		//这里的公式可以简化 相约

		return rbw;  
	}



	static std::array<int,3> getToneFromPSD(vector<double> pxx, vector<double> freqs, int size, double baseFreq, int order)
	{
		//这里计算出来的是目标频率，基频乘以谐波阶数
		double freq = baseFreq * order;

		int maxToneIndex;  //最大值所在的位置
		double maxTone;   //最大的谐波

		if (order == 1)
		{
			auto iter = std::max_element(pxx.begin(), pxx.begin() + size);
			maxToneIndex = std::distance(pxx.begin(), iter);
			maxTone = *iter;
		}
		else if (freq >= freqs.front() && freq <= freqs.back())
		{
			//查找最近的频点
			auto iter = std::lower_bound(freqs.begin(), freqs.begin() + size, freq);
			auto iter2 = iter - 1;
			if (*iter - freq > freq - *iter2)
			{
				iter = iter2;  //说明前一个点更接近freq
			}
			
			int index = std::distance(freqs.begin(), iter);
			//按照左右分离 计算边界
			int left = std::max(0, index - 1);
			int right = std::min(index + 1, int(freqs.size() - 1));

			iter = std::max_element(pxx.begin() + left, pxx.begin() + right + 1);
			//在分离的范围内获取最大值，刷新标志位
			maxToneIndex = std::distance(pxx.begin(), iter);
			maxTone = *iter;
		}

		int left = maxToneIndex - 1;
		int right = maxToneIndex + 1;

		//left 计算左侧第一个开始变大的点，  right计算右侧第一个开始变大的点
		while (left >= 0 && pxx[left] <= pxx[left+1])
		{
			left -= 1;
		}

		while (right <= pxx.size() && pxx[right - 1] >= pxx[right])
		{
			right += 1;
		}


		return std::array<int, 3>{left, maxToneIndex, right};

	}

	static std::pair<double, double> getPowerFreq(vector<double> pxx, vector<double> freqs, int size, double rbw, std::array<int, 3>indexs)
	{
		auto[left, toneIndex, right] = indexs;

		auto width = freqs[1] - freqs[0];

		double power = 0;
		double sum = 0;

		double calcFreq = 0.0; //计算出来的频率

		for (int i = left; i <= right; i++)
		{
			power += pxx[i] * freqs[i];
			sum += pxx[i];
		}

		calcFreq = power / sum;  //计算出来的基准频率  

		power = 0;
		if (left < right)
		{
			for(int i= left; i<= right; i++)
			{
				power += pxx[i] * width;
			}
		}else if(right > 0 && right < size)
		{
			power = pxx[right] *(freqs[right + 1] - freqs[right - 1]) / 2;
		}else
		{
			power = pxx[right] * width;
		}

		if(power < rbw * pxx[toneIndex])
		{
			power = rbw *pxx[toneIndex];
			calcFreq = freqs[toneIndex];
		}


		return {calcFreq, power};
	}


	//计算谐波
	static std::pair<double,double> computeHarm(vector<double>pxx, vector<double> freqs, int size, double rbw,  double baseFreq,  int order )
	{

		double targetFreq = baseFreq * order; //目标频率

		if (order == 1)
		{
			pxx[0] *= 2;

			auto[left, toneIndex, right] = getToneFromPSD(pxx, freqs, size, baseFreq, 0);
			left +=1;
			right -=1;

			for (int i = left; i <= right; i++)
			{
				pxx[i] = 0;  //对基频清0
			}


			auto[left2, toneIndex2, right2] = getToneFromPSD(pxx, freqs, size, baseFreq, 1);
			left2 +=1;
			right2 -=1;

			auto ress = getPowerFreq(pxx, freqs, size, rbw, {left2, toneIndex2, right2});  //获取频率

			for (int i = left2; i <= right2; i++)
			{
				pxx[i] = 0;
			}
			return ress;
		}
		else
		{
			auto res = getToneFromPSD(pxx, freqs, size, baseFreq, 1);
			int left2 = res[0] + 1;
			int toneIndex2 = res[1];
			int right2 = res[2] - 1;

			auto ress = getPowerFreq(pxx, freqs, size, rbw, {left2, toneIndex2, right2});  //获取频率
			return ress;
		}



	}





};


