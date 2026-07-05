#include<cmath>
#include<algorithm>
#include "FindDelay.h"
#include"fftw3.h"

int FindDelay::corr_delay(std::vector<float> vec1, std::vector<float> vec2)
{
	auto size1 = vec1.size();
	auto size2 = vec2.size();

	//要求长度相等

	int max_lag = static_cast<int>(size1 - 1);
	std::vector<float> corr(2 * size1 - 1, 0.0f);

	for (int lag = -max_lag; lag <= max_lag; ++lag)
	{
		float sum = 0.0f;
		int count = 0;
		for (int i = 0; i < size1; ++i)
		{
			int j = i - lag;
			if (j >= 0 && j < size1)
			{
				sum += vec1[i] * vec2[j];
				count++;
			}
		}

		corr[lag + max_lag] = sum;  //除以count可以尽心归一化
	}

	auto it = std::max_element(corr.begin(), corr.end());
	int max_idx = std::distance(corr.begin(), it);

	max_idx -= max_lag;




	return 0;
}


int  FindDelay::gcc_phat_delay(const std::vector<float>& x1, const std::vector<float>& x2) {
    if (x1.size() != x2.size()) {
        //throw std::invalid_argument("信号长度必须相等");
    }
    size_t N = x1.size();

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
        in1[i][0] = (i < N) ? x1[i] : 0.0;
        in1[i][1] = 0.0;
        in2[i][0] = (i < N) ? x2[i] : 0.0;
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

