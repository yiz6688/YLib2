#include<cmath>
#include<algorithm>
#include "FindDelay.h"
#include"fftw3.h"
#include<complex>
#include<bit>
#include<memory>

using complex = std::complex<double>;

int FindDelay::corr_delay(std::span<double> x, std::span<double> y, int maxLag)
{
	auto xLen = x.size();
	auto yLen=  y.size();
    auto sampleNum = xLen + yLen - 1;
    int fftSize =  std::bit_ceil(sampleNum);  //等价于nextpow2
    int M = fftSize / 2 + 1;



    std::vector<double> in(sampleNum);  //缓冲区

    //std::unique_ptr<double> in = std::make_unique<double>(sampleNum);
    fftw_complex* out = fftw_alloc_complex(M);

    complex* X = reinterpret_cast<complex*> (out);
    std::vector<complex> temp(M);

    auto pfft = fftw_plan_dft_r2c_1d(fftSize, in.data(), out, FFTW_ESTIMATE);

    auto pifft = fftw_plan_dft_c2r_1d(fftSize, out, in.data(), FFTW_ESTIMATE);

    std::fill_n(in.begin(), sampleNum, 0.0); //清0
    std::copy_n(x.begin(), xLen, in.begin()); //copy x到输入

    fftw_execute(pfft);  //fft x
    std::copy_n(X, M, temp.data());  //拷贝过去

    std::fill_n(in.begin(), sampleNum, 0.0); //清0
    std::copy_n(y.begin(), yLen, in.begin()); //copy y到输入

    fftw_execute(pfft); //fft y

    for(int i=0; i< M ;i++)
    {
        X[i] *= std::conj(temp[i]);  //x*conj(y)  共轭乘。
    }

    fftw_execute(pifft); //ifft 结果是互相关序列， 结果在 in里面

    fftw_destroy_plan(pfft);
    fftw_destroy_plan(pifft);

    fftw_free(out);



	return 0;
}


int  FindDelay::gcc_phat_delay(std::span<double> x, std::span<double> y) {
    if (x.size() != y.size()) {
        //throw std::invalid_argument("信号长度必须相等");
    }
    size_t N = x.size();

    // 零填充到 2N 以避免循环卷积（提高分辨率）
    size_t fft_size = 1;
    while (fft_size < 2 * N) fft_size <<= 1;

    // 分配内存
    fftw_complex* in1 = fftw_alloc_complex(fft_size);
    fftw_complex* in2 = fftw_alloc_complex(fft_size);
    fftw_complex* out1 = fftw_alloc_complex(fft_size);
    fftw_complex* out2 = fftw_alloc_complex(fft_size);
    double* corr = fftw_alloc_real(fft_size);

    // 填充输入（其余补零）
    for (size_t i = 0; i < fft_size; ++i) {
        in1[i][0] = (i < N) ? x[i] : 0.0;
        in1[i][1] = 0.0;
        in2[i][0] = (i < N) ? y[i] : 0.0;
        in2[i][1] = 0.0;
    }

    // 创建 FFT 计划
    fftw_plan p1 = fftw_plan_dft_1d(fft_size, in1, out1, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_plan p2 = fftw_plan_dft_1d(fft_size, in2, out2, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_plan p3 = fftw_plan_dft_c2r_1d(fft_size, in1, corr, FFTW_BACKWARD);

    // 执行正向 FFT
    fftw_execute(p1);
    fftw_execute(p2);

    // 计算 PHAT 加权：X1 * conj(X2) / |X1 * conj(X2)|
    for (size_t k = 0; k < fft_size; ++k) {
        double re1 = out1[k][0], im1 = out1[k][1];
        double re2 = out2[k][0], im2 = out2[k][1];

        // conj(X2) = re2 - j*im2
        double cross_re = re1 * re2 + im1 * im2;  // real(X1 * conj(X2))
        double cross_im = im1 * re2 - re1 * im2;  // imag(X1 * conj(X2))

        double magnitude = std::sqrt(cross_re * cross_re + cross_im * cross_im);

        // 避免除零：若幅度太小，设为 0（或跳过）
        if (magnitude < 1e-12) {
            in1[k][0] = 0.0;
            in1[k][1] = 0.0;
        }
        else {
            // 归一化为单位复数（仅保留相位）
            in1[k][0] = cross_re / magnitude;  // 实部
            in1[k][1] = cross_im / magnitude;  // 虚部
        }
    }

    // 执行逆 FFT（注意：FFTW_BACKWARD 不做 1/N 归一化）
    fftw_execute(p3);

    // 归一化（可选，不影响峰值位置）
    for (size_t i = 0; i < fft_size; ++i) {
        corr[i] /= fft_size;
    }

    // 在有效范围 [0, 2*N-1] 内找最大值
    size_t max_idx = 0;
    double max_val = corr[0];
    size_t search_len = 2 * N - 1;
    for (size_t i = 1; i < search_len; ++i) {
        if (corr[i] > max_val) {
            max_val = corr[i];
            max_idx = i;
        }
    }

    // 转换为时延（索引 0 ~ N-1：x2 超前；N ~ 2N-2：x2 滞后）
    int delay = static_cast<int>(max_idx);
    if (delay > static_cast<int>(N - 1)) {
        delay -= static_cast<int>(fft_size); // 处理负延迟（可选）
        // 但通常我们只关心 [-N+1, N-1]，所以也可以直接：
        // delay = delay - (N - 1);
    }
    // 更直观的方式：中心在 N-1
    delay = static_cast<int>(max_idx) - static_cast<int>(N - 1);

    // 清理资源
    fftw_destroy_plan(p1);
    fftw_destroy_plan(p2);
    fftw_destroy_plan(p3);
    fftw_free(in1);
    fftw_free(in2);
    fftw_free(out1);
    fftw_free(out2);
    fftw_free(corr);

    return delay;
}

