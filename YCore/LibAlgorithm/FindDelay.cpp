#include<cmath>
#include<algorithm>
#include "FindDelay.h"
#include"fftw3.h"
#include<complex>
#include<bit>
#include<span>
#include<memory>
#include"FFT.h"
#include<print>


std::vector<double> FindDelay::correlate(std::span<double> x, std::span<double> y)
{
	auto xLen = x.size();
	auto yLen=  y.size();
    auto sampleNum = xLen + yLen - 1;
    int fftSize =  std::bit_ceil(sampleNum);  //等价于nextpow2
    int M = fftSize / 2 + 1;



    double* din = fftw_alloc_real(fftSize);
    fftw_complex* dout = fftw_alloc_complex(M);

    cpx* X = reinterpret_cast<cpx*> (dout);  //转换指针为标准库类型
    std::vector<cpx> temp(M);       //缓存

    

    //正向FFT
    auto pfft = fftw_plan_dft_r2c_1d(fftSize, din, dout, FFTW_ESTIMATE);
    //互相关之后的逆向FFT
    auto pifft = fftw_plan_dft_c2r_1d(fftSize, dout, din, FFTW_ESTIMATE);


    std::fill_n(din, fftSize, 0.0); //清0
    std::copy_n(x.begin(), xLen, din); //copy x到输入

    fftw_execute(pfft);  //fft x
    std::copy_n(X, M, temp.data());  //拷贝到临时缓冲区

    std::fill_n(din, fftSize, 0.0); //清0
    std::copy_n(y.begin(), yLen, din); //copy y到输入

    fftw_execute(pfft); //fft y

    for(int i=0; i< M ;i++)
    {
        X[i] = temp[i] * std::conj(X[i]);//x*conj(y)  共轭乘。
    }

    fftw_execute(pifft); //ifft 结果是互相关序列， 结果在 in里面


    std::vector<double> corr(xLen + yLen -1); 
    int offset = 0;
    for(int i = fftSize - yLen + 1; i < fftSize; i++)
    {
        corr[offset++] = din[i] / fftSize;
    }

    for(int i = 0; i< xLen; i++)
    {
        corr[offset++] = din[i] / fftSize;
    }


    //释放资源
    fftw_destroy_plan(pfft);
    fftw_destroy_plan(pifft);

    fftw_free(din);
    fftw_free(dout);

	return corr;
}

std::vector<double> FindDelay::correlate2(std::span<double> x, std::span<double> y)
{
	auto xLen = x.size();
	auto yLen=  y.size();
    auto sampleNum = xLen + yLen - 1;
    int fftSize =  std::bit_ceil(sampleNum);  //等价于nextpow2

    FFT fft(fftSize);
    
    auto TD = fft.getTD(); //指向时域
    auto FD = fft.getFD(); //指向频域
    std::vector<cpx> temp(FD.size());       //缓存

    fft.fft(x);
    std::copy(FD.begin(), FD.end(), temp.begin());  //缓存x的fft结果

    fft.fft(y);

    for(int i=0; i< FD.size() ;i++)
    {
        FD[i] = temp[i] * std::conj(FD[i]);//x*conj(y)  共轭乘。
    }

    fft.ifft(FD);

    std::vector<double> corr(xLen + yLen -1); 
    int offset = 0;
    for(int i = fftSize - yLen + 1; i < fftSize; i++)
    {
        corr[offset++] = TD[i] / fftSize;
    }

    for(int i = 0; i< xLen; i++)
    {
        corr[offset++] = TD[i] / fftSize;
    }

	return corr;
}

int  FindDelay::gcc_phat_delay(std::span<double> x, std::span<double> y) 
{
	auto xLen = x.size();
	auto yLen=  y.size();
    auto M = std::max(xLen, yLen);
    auto sampleNum = 2*M - 1;
    int fftSize =  std::bit_ceil(static_cast<unsigned>(sampleNum));  //等价于nextpow2

    FFT2 fft(fftSize);

    auto TD = fft.getTD(); //指向时域
    auto FD = fft.getFD(); //指向频域
    std::vector<cpx> temp(FD.size());       //缓存
    
    
    fft.fft(x);
    std::copy(FD.begin(), FD.end(), temp.begin());  //缓存x的fft结果

    fft.fft(y);

    for(int i=0; i< FD.size() ;i++)
    {
        FD[i] = temp[i] * std::conj(FD[i]);//x*conj(y)  共轭乘。
        FD[i] /= abs(FD[i]);
    }

    fft.ifft(FD);

    // for(int i=0; i< fftSize; i++)
    // {
    //     std::println("{:.12f},{:.12f}", TD[i].real(), TD[i].imag());
    // }

    std::vector<double> corr(xLen + yLen - 1); 
    int offset = 0;
    for(int i = fftSize - yLen + 1; i < fftSize; i++)
    {
        corr[offset++] = abs(TD[i]);
    }

    for(int i = 0; i< xLen; i++)
    {
        corr[offset++] = abs(TD[i]);
    }

    auto iter = std::max_element(corr.begin(), corr.end());
    auto pos = std::distance(corr.begin(), iter);

    auto delay = pos - yLen + 1;


	return delay;
}

