#pragma once
#include<vector>
#include<array>
#include<cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//余弦窗
class CosineWindow
{
public:    
    //这些窗系数使用通用方式里面进行
    static constexpr std::array<double, 5> Rect{ 1.0 };                       //特例余弦窗
    static constexpr std::array<double, 5> Hann{ 0.5, -0.5 };                 //端点完全降为0， 旁瓣衰减-31db
    static constexpr std::array<double, 5> Hamming{ 0.54, -0.46 };            //端点不为0 大于0.08，最小化第一旁瓣 大约-41db
    static constexpr std::array<double, 5> Blackman{ 0.42, -0.5, 0.08 };      //旁瓣大约-58db，主瓣较宽
    static constexpr std::array<double, 5> Blackman_Harris{ 0.35875, -0.48829, 0.14128, -0.01168 };  //旁瓣约等于-92db，常用于高动态范围分析
    static constexpr std::array<double, 5> Nuttall{ 0.3635819, -0.4891775, 0.1365995, -0.0106411 }; //旁瓣约等于-93db，比哈里斯更平滑
    static constexpr std::array<double, 5> FlatTop{ 1.0, -1.9308742, 1.2864454, -0.3878014, 0.0322299 };  //主瓣极宽，但通带平坦，用于幅度精确测量
    //1.0, -1.93, 1.29, -0.388, 0.028

	
public:

	std::vector<double> getWindiow(int N, const std::array<double, 5>& coef)
	{
		//ofstream ofs("D:\\123.txt");
		std::vector<double> win(N);
		double fact = 1.0 * 2 * M_PI / N;
		for (int i = 0; i < N; i++)
		{
			win[i] = coef[0] + coef[1] * cos(fact * i) + coef[2] * cos(2 * fact * i)
				+ coef[3] * cos(3 * fact * i) + coef[4] * cos(4 * fact * i);
			//ofs << win[i] << endl;
		}

		return win;
	}



    void cccc()
    {
        //auto v1 = std::accumulate(res.begin(), res.end(), 0.0);

       // auto v2 = std::accumulate(res.begin(), res.end(), 0.0, [](auto a, auto b) {return a + b * b; });

       // 幅度恢复 = avg / winLen;
       // 能量恢复 = sqrt(power / winLen);           fft计算的结果除以恢复系数获取对应的内容

        //cout << i << "  " << i / v1 << "  " << std::sqrt(i / v2) << endl;
    }


    /**
 * @brief 生成通用余弦窗（Generalized Cosine Window）
 *
 * 窗函数公式：
 *   w(n) = sum_{k=0}^{K} a_k * cos(2π * k * n / (N - 1))
 * 其中 n = 0, 1, ..., N-1
 *
 * @param N       窗长度（必须 > 0）
 * @param coeffs  系数向量 {a0, a1, a2, ..., aK}，大小为 K+1
 * @return        长度为 N 的窗函数值（std::vector<double>）
 */
    std::vector<double> generalized_cosine_window(int N, const std::vector<double>& coeffs) {
        if (N <= 0 || coeffs.empty()) {
            return {};
        }

        std::vector<double> window(N, 0.0);
        int K = static_cast<int>(coeffs.size()) - 1; // 最高阶数

        // 特殊情况：N == 1，避免除零
        if (N == 1) {
            double sum = 0.0;
            for (double a : coeffs) sum += a; // 因为 cos(0) = 1 对所有 k
            window[0] = sum;
            return window;
        }

        const double denom = static_cast<double>(N - 1);

        double fact = 1.0 * 2 * M_PI / (N - 1);

        for (int n = 0; n < N; ++n) {
            double value = 0.0;
            for (int k = 0; k <= K; ++k) {
                value += coeffs[k] * std::cos(2.0 * M_PI * k * static_cast<double>(n) / denom);
            }
            window[n] = value;
        }

        return window;
    }





};