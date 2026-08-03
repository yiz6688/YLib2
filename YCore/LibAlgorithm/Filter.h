#pragma once
#include<numbers>
#include<cmath>
#include<print>
#include<complex>

#ifndef M_PI
	constexpr auto M_PI = std::numbers::pi;
	//#define M_PI 3.14159265358979323846
#endif

using cpx = std::complex<double>;
struct ccc
{
	double a0;
	double a1;
	double a2;
	double b0;
	double b1;
	double b2;
};





class Biquad {



	ccc PeakFilter(double centerFreq, double Q, double peakGainDB, double sampleRate)
	{
		ccc coeffs;
		double A = pow(10, peakGainDB / 40);
		double omega = 2 * M_PI * centerFreq / sampleRate;
		double alpha = sin(omega) / (2 * Q);
		double cosw = cos(omega);
		coeffs.b0 = 1 + alpha * A;
		coeffs.b1 = -2 * cosw;
		coeffs.b2 = 1 - alpha * A;
		coeffs.a0 = 1 + alpha / A;
		coeffs.a1 = -2 * cosw;
		coeffs.a2 = 1 - alpha / A;
		// Normalize the coefficients
		coeffs.b0 /= coeffs.a0;
		coeffs.b1 /= coeffs.a0;
		coeffs.b2 /= coeffs.a0;
		coeffs.a1 /= coeffs.a0;
		coeffs.a2 /= coeffs.a0;
		return coeffs;
	}


	ccc NotchFilter(double centerFreq, double Q, double sampleRate)
	{
		ccc coeffs;
		double omega = 2 * M_PI * centerFreq / sampleRate;
		double alpha = sin(omega) / (2 * Q);
		double cosw = cos(omega);
		coeffs.b0 = 1;
		coeffs.b1 = -2 * cosw;
		coeffs.b2 = 1;
		coeffs.a0 = 1 + alpha;
		coeffs.a1 = -2 * cosw;
		coeffs.a2 = 1 - alpha;
		// Normalize the coefficients
		coeffs.b0 /= coeffs.a0;
		coeffs.b1 /= coeffs.a0;
		coeffs.b2 /= coeffs.a0;
		coeffs.a1 /= coeffs.a0;
		coeffs.a2 /= coeffs.a0;
		return coeffs;
	}


	ccc LowPassFilter(double cutoffFreq, double Q, double sampleRate)
	{
		ccc coeffs;
		double omega = 2 * M_PI * cutoffFreq / sampleRate;
		double alpha = sin(omega) / (2 * Q);
		double cosw = cos(omega);
		coeffs.b0 = (1 - cosw) / 2;
		coeffs.b1 = 1 - cosw;
		coeffs.b2 = (1 - cosw) / 2;
		coeffs.a0 = 1 + alpha;
		coeffs.a1 = -2 * cosw;
		coeffs.a2 = 1 - alpha;
		// Normalize the coefficients
		coeffs.b0 /= coeffs.a0;
		coeffs.b1 /= coeffs.a0;
		coeffs.b2 /= coeffs.a0;
		coeffs.a1 /= coeffs.a0;
		coeffs.a2 /= coeffs.a0;
		return coeffs;
	}

	ccc HighPassFilter(double cutoffFreq, double Q, double sampleRate)
	{
		ccc coeffs;
		double omega = 2 * M_PI * cutoffFreq / sampleRate;
		double alpha = sin(omega) / (2 * Q);
		double cosw = cos(omega);
		coeffs.b0 = (1 + cosw) / 2;
		coeffs.b1 = -(1 + cosw);
		coeffs.b2 = (1 + cosw) / 2;
		coeffs.a0 = 1 + alpha;
		coeffs.a1 = -2 * cosw;
		coeffs.a2 = 1 - alpha;
		// Normalize the coefficients
		coeffs.b0 /= coeffs.a0;
		coeffs.b1 /= coeffs.a0;
		coeffs.b2 /= coeffs.a0;
		coeffs.a1 /= coeffs.a0;
		coeffs.a2 /= coeffs.a0;
		return coeffs;
	}


	ccc BandPassFilter(double centerFreq, double Q, double sampleRate)
	{
		ccc coeffs;
		double omega = 2 * M_PI * centerFreq / sampleRate;
		double alpha = sin(omega) / (2 * Q);
		double cosw = cos(omega);
		coeffs.b0 = alpha;
		coeffs.b1 = 0;
		coeffs.b2 = -alpha;
		coeffs.a0 = 1 + alpha;
		coeffs.a1 = -2 * cosw;
		coeffs.a2 = 1 - alpha;
		// Normalize the coefficients
		coeffs.b0 /= coeffs.a0;
		coeffs.b1 /= coeffs.a0;
		coeffs.b2 /= coeffs.a0;
		coeffs.a1 /= coeffs.a0;
		coeffs.a2 /= coeffs.a0;
		return coeffs;
	}

	ccc LowShelfFilter(double cutoffFreq, double S, double gainDB, double sampleRate)
	{
		ccc coeffs;
		double A = pow(10, gainDB / 40);
		double omega = 2 * M_PI * cutoffFreq / sampleRate;
		double alpha = sin(omega) / 2 * sqrt((A + 1 / A) * (1 / S - 1) + 2);
		double cosw = cos(omega);
		coeffs.b0 = A * ((A + 1) - (A - 1) * cosw + 2 * sqrt(A) * alpha);
		coeffs.b1 = 2 * A * ((A - 1) - (A + 1) * cosw);
		coeffs.b2 = A * ((A + 1) - (A - 1) * cosw - 2 * sqrt(A) * alpha);
		coeffs.a0 = (A + 1) + (A - 1) * cosw + 2 * sqrt(A) * alpha;
		coeffs.a1 = -2 * ((A - 1) + (A + 1) * cosw);
		coeffs.a2 = (A + 1) + (A - 1) * cosw - 2 * sqrt(A) * alpha;
		// Normalize the coefficients
		coeffs.b0 /= coeffs.a0;
		coeffs.b1 /= coeffs.a0;
		coeffs.b2 /= coeffs.a0;
		coeffs.a1 /= coeffs.a0;
		coeffs.a2 /= coeffs.a0;
		return coeffs;
	}

	ccc HighShelfFilter(double cutoffFreq, double S, double gainDB, double sampleRate)
	{
		ccc coeffs;
		double A = pow(10, gainDB / 40);
		double omega = 2 * M_PI * cutoffFreq / sampleRate;
		double alpha = sin(omega) / 2 * sqrt((A + 1 / A) * (1 / S - 1) + 2);
		double cosw = cos(omega);
		coeffs.b0 = A * ((A + 1) + (A - 1) * cosw + 2 * sqrt(A) * alpha);
		coeffs.b1 = -2 * A * ((A - 1) + (A + 1) * cosw);
		coeffs.b2 = A * ((A + 1) + (A - 1) * cosw - 2 * sqrt(A) * alpha);
		coeffs.a0 = (A + 1) - (A - 1) * cosw + 2 * sqrt(A) * alpha;
		coeffs.a1 = 2 * ((A - 1) - (A + 1) * cosw);
		coeffs.a2 = (A + 1) - (A - 1) * cosw - 2 * sqrt(A) * alpha;
		// Normalize the coefficients
		coeffs.b0 /= coeffs.a0;
		coeffs.b1 /= coeffs.a0;
		coeffs.b2 /= coeffs.a0;
		coeffs.a1 /= coeffs.a0;
		coeffs.a2 /= coeffs.a0;
		return coeffs;
	}


	void getValue(double freq, ccc c, double sampleRate)
	{
		auto[a0, a1, a2, b0, b1, b2] = c;

		double omega = 2 * M_PI * freq / sampleRate;
		double cosw = cos(omega);
		double sinw = sin(omega);

		cpx numerator = c.b0 + c.b1 * cpx(sinw, cosw) + c.b2 * cpx(sin(2 * omega), cos(2 * omega));
		cpx denominator = c.a0 + c.a1 * cpx(sinw, cosw) + c.a2 * cpx(sin(2 * omega), cos(2 * omega));

		double magnitude = std::abs(numerator / denominator);

		double phase = atan2(std::imag(numerator / denominator), std::real(numerator / denominator));

		//std::cout << "Magnitude: " << magnitude << std::endl;
		//std::cout << "Phase: " << phase << std::endl;
	}


	//获取db值
	double getValue2(double freq, ccc c, double fs)
	{
		auto w = 2 * M_PI * freq / fs;
		auto sinw2 = sin(w / 2) * sin(w / 2);
		auto[a0, a1, a2, b0, b1, b2] = c;

		auto num2 =(b0 + b1 + b2) * (b0 + b1 + b2) - 4 * (b0 * b1 + 4 * b0 * b2 + b1 * b2) * sinw2 
			+ 16 * b0 * b2 * sinw2 * sinw2;	
		
		auto den2 = (a0 + a1 + a2) * (a0 + a1 + a2) - 4 * (a0 * a1 + 4 * a0 * a2 + a1 * a2) * sinw2
			+ 16 * a0 * a2 * sinw2 * sinw2;

		
		return 10 * log10(num2 / den2); //音频是20log(v), 这里是平方，就不用20了。
	}

};