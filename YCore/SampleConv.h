#pragma once
#include<cmath>
#include<limits>
#include<random>
#include"myType.h"
#define COMPATIBILITY_MODE


struct Channel
{
	int index = 0;
	int total = 1;
};

class SampleConv
{

private:
	static constexpr int INT24_MAX = 8388607;
	static constexpr int INT24_MIN = -8388608;

public:
	template<typename T>
	static void IntToFloat(T* src, int samplelen, float* dest, float scale = 1.0f)
	{
		#ifndef COMPATIBILITY_MODE
			constexpr int q_min = (std::numeric_limits<T>::min)();   //量化为最小值上下对称
			const float coeff = -1.0f * scale / q_min ;  //用最小值也就是大的值进行转浮点数
		#else
			constexpr int q_max = (std::numeric_limits<T>::max)();
			const float coeff = 1.0f * scale / q_max ;  //用最大值也就是大的值进行转浮点数
		#endif
			
		T* ptr1 = src + samplelen - 1;
		float* ptr2 = dest + samplelen - 1;

		for (int i = 0; i < samplelen; i++)
		{
			*ptr2 = (*ptr1) * coeff * scale;
			ptr1--;
			ptr2--;
		}
	}

	template<typename T>
	static void IntToFloat(T* src, int samplelen, float* dest, int nChannel = 1)
	{
		#ifndef COMPATIBILITY_MODE
			constexpr int q_min = (std::numeric_limits<T>::min)();   //量化为最小值上下对称
			const float coeff = -1.0f / q_min ;  //用最小值也就是大的值进行转浮点数
		#else
			constexpr int q_max = (std::numeric_limits<T>::max)();
			const float coeff = 1.0f / q_max ;  //用最大值也就是大的值进行转浮点数
		#endif
			
		T* ptr1 = src + samplelen - 1;
		float* ptr2 = dest + samplelen - 1;

		for (int i = 0; i < samplelen; i+=nChannel)
		{
			*ptr2 = (*ptr1) * coeff ;
			ptr1-=nChannel;
			ptr2--;
		}
	}

	static void IntToFloat(int24* src, int samplelen, float* dest, float scale = 1.0f)
	{
		#ifndef COMPATIBILITY_MODE
			constexpr int q_min = (std::numeric_limits<int24>::max)();   //量化为最小值上下对称
			const float coeff = -1.0f * scale / q_min ;  //用最小值也就是大的值进行转浮点数
		#else
			constexpr int q_max = (std::numeric_limits<int24>::max)();
			const float coeff = 1.0f * scale / q_max ;  //用最大值也就是大的值进行转浮点数
		#endif
			
		int24* ptr1 = src + samplelen - 1;
		float* ptr2 = dest + samplelen - 1;

		for (int i = 0; i < samplelen; i++)
		{
			*ptr2 = (*ptr1) * coeff * scale;
			ptr1--;
			ptr2--;
		}
	}

	template<typename T>
	static void IntToFloat(char* src, int samplelen, float*dest)
	{
		#ifndef COMPATIBILITY_MODE
			constexpr int q_min = (std::numeric_limits<T>::min)();   //量化为最小值上下对称
			const float coeff = -1.0f / q_min ;  //用最小值也就是大的值进行转浮点数
		#else
			constexpr int q_max = (std::numeric_limits<T>::max)();
			const float coeff = 1.0f / q_max ;  //用最大值也就是大的值进行转浮点数
		#endif
			
		char* ptr1 = src + samplelen * sizeof(T) - 1;
		float* ptr2 = dest + samplelen - 1;

		for (int i = 0; i < samplelen; i++)
		{
			T value = 0;
			for(int k = 0; k < sizeof(T); k++)
			{
				value = (value << 8) | (*ptr1 & 0xFF);
				ptr1--;
			}
			*ptr2 = value * coeff ;
			ptr2--;
		}
	}


	template<typename T>
	static void IntToDouble(T* src, int samplelen, double* dest, double scale = 1.0)
	{
		#ifndef COMPATIBILITY_MODE
			constexpr int q_min = (std::numeric_limits<T>::min)();   //量化为最小值上下对称
			const double coeff = -1.0 * scale / q_min ;  //用最小值也就是大的值进行转浮点数
		#else
			constexpr int q_max = (std::numeric_limits<T>::max)();
			const double coeff = 1.0 * scale / q_max ;  //用最大值也就是大的值进行转浮点数
		#endif

		T* ptr1 = src + samplelen - 1;
		double* ptr2 = dest + samplelen - 1;

		for (int i = 0; i < samplelen; i++)
		{
			*ptr2 = (*ptr1) * coeff * scale;
			ptr1--;
			ptr2--;
		}
	}

	static void IntToFloat(int24* src, int samplelen, double* dest, double scale = 1.0f)
	{
		#ifndef COMPATIBILITY_MODE
			constexpr int q_min = INT24_MIN;   //量化为最小值上下对称
			const float coeff = -1.0f * scale / q_min ;  //用最小值也就是大的值进行转浮点数
		#else
			constexpr int q_max = INT24_MAX;
			const float coeff = 1.0f * scale / q_max ;  //用最大值也就是大的值进行转浮点数
		#endif
			
		int24* ptr1 = src + samplelen - 1;
		double* ptr2 = dest + samplelen - 1;

		for (int i = 0; i < samplelen; i++)
		{
			*ptr2 = (*ptr1) * coeff * scale;
			ptr1--;
			ptr2--;
		}
	}



	//3字节整数保存在4字节整数中，保存逻辑是空出最低位的1个字节，这样保持符号一致性
	static void Int24ToFloat(int* src, int samplelen, float* dest, float scale = 1.0f)
	{
		const int q_min = -8388608;   //量化为最小值上下对称
		const int q_max = -q_min;
		const float coeff = 1.0f * scale / q_max;  //用最小值也就是大的值进行转浮点数
		int* ptr1 = src + samplelen - 1;
		float* ptr2 = dest + samplelen - 1;

		for (int i = 0; i < samplelen; i++)
		{
			*ptr2 = (*ptr1) * coeff * scale;
			ptr1--;
			ptr2--;
		}
	}

	template<typename T>
	static void FloatToInt(float* src, int samplelen, T* dest, float scale = 1.0f)
	{
		constexpr int q_max = (std::numeric_limits<T>::max)();   //量化为最大值上下对称
		constexpr int q_min = (std::numeric_limits<T>::min)();
		const int coeff = q_max;  //用最大值也就是大的值进行转浮点数,没有歧义

		float* ptr1 = src + samplelen - 1;
		T* ptr2 = dest + samplelen - 1;
		for (int i = 0; i < samplelen; i++)
		{
			auto value = static_cast<int>(std::round((*ptr1) * coeff * scale));
			if (value < q_min)
			{
				value = q_min;
			}
			else if(value > q_max)
			{
				value = q_max;
			}

			*ptr2 = value;
			ptr1--;
			ptr2--;
		}
	}

	static void FloatToInt(float* src, int samplelen, int24* dest, float scale = 1.0f)
	{
		constexpr int q_max = INT24_MAX;   //量化为最大值上下对称
		constexpr int q_min = INT24_MIN;
		const int coeff = q_max;  //用最大值也就是大的值进行转浮点数,没有歧义

		float* ptr1 = src + samplelen - 1;
		int24* ptr2 = dest + samplelen - 1;
		for (int i = 0; i < samplelen; i++)
		{
			auto value = static_cast<int>(std::round((*ptr1) * coeff * scale));
			if (value < q_min)
			{
				value = q_min;
			}
			else if(value > q_max)
			{
				value = q_max;
			}

			*ptr2 = value;
			ptr1--;
			ptr2--;
		}
	}

	template<typename T>
	static void DoubleToInt(double* src, int samplelen, T* dest, double scale = 1.0)
	{
		constexpr int q_max = (std::numeric_limits<T>::max)();   //量化为最大值上下对称
		constexpr int q_min = (std::numeric_limits<T>::min)();
		const int coeff = q_max;  //用最大值也就是大的值进行转浮点数

		double* ptr1 = src + samplelen - 1;
		T* ptr2 = dest + samplelen - 1;
		for (int i = 0; i < samplelen; i++)
		{
			auto value = static_cast<int>(std::round((*ptr1) * coeff * scale));
			if (value < q_min)
			{
				value = q_min;
			}
			else if(value > q_max)
			{
				value = q_max;
			}

			*ptr2 = value;
			ptr1--;
			ptr2--;
		}
	}

	static void DoubleToInt(double* src, int samplelen, int24* dest, float scale = 1.0f)
	{
		constexpr int q_max = INT24_MAX;   //量化为最大值上下对称
		constexpr int q_min = INT24_MIN;
		const int coeff = q_max;  //用最大值也就是大的值进行转浮点数,没有歧义

		double* ptr1 = src + samplelen - 1;
		int24* ptr2 = dest + samplelen - 1;
		for (int i = 0; i < samplelen; i++)
		{
			auto value = static_cast<int>(std::round((*ptr1) * coeff * scale));
			if (value < q_min)
			{
				value = q_min;
			}
			else if(value > q_max)
			{
				value = q_max;
			}

			*ptr2 = value;
			ptr1--;
			ptr2--;
		}
	}


	static void FloatToInt24(float* src, int samplelen, int* dest, float scale = 1.0f)
	{
		const int q_max = 8388607;   //量化为最大值上下对称
		const int q_min = -8388608;
		const int coeff = q_max;  //用最大值也就是大的值进行转浮点数
		float* ptr1 = src + samplelen - 1;
		int* ptr2 = dest + samplelen - 1;
		for (int i = 0; i < samplelen; i++)
		{
			auto value = static_cast<int>(std::round((*ptr1) * coeff * scale));
			if (value < q_min)
			{
				value = q_min;
			}
			else if (value > q_max)
			{
				value = q_max;
			}

			*ptr2 = value;
			ptr1--;
			ptr2--;
		}
	}

	static void DoubleToInt24(double* src, int samplelen, int* dest, double scale = 1.0)
	{
		const int q_max = 8388607;   //量化为最大值上下对称
		const int q_min = -8388608;
		const int coeff = q_max;  //用最大值也就是大的值进行转浮点数
		double* ptr1 = src + samplelen - 1;
		int* ptr2 = dest + samplelen - 1;
		for (int i = 0; i < samplelen; i++)
		{
			auto value = static_cast<int>(std::round((*ptr1) * coeff * scale));
			if (value < q_min)
			{
				value = q_min;
			}
			else if (value > q_max)
			{
				value = q_max;
			}

			*ptr2 = value;
			ptr1--;
			ptr2--;
		}
	}


public:
	//16位整数转float
	static void Int16toFloat(short* src, int samplelen, float* dest)
	{
		IntToFloat(src, samplelen, dest, 1.0f);
	}
	//传入采样点长度
	//24位整数转float
	void static Int24BytetoFloat(char* src, int samplelen, float* dest)
	{
		//const int q_max = 8388607;   //量化为最大值上下对称
		const int q_min = -8388608;
		float coeff = -1.0f / q_min;

		char* ptr1 = src + (samplelen - 1) * 3;//-3 偏移
		float* ptr2 = dest + samplelen - 1;

		int value = 0;
		for (int i = 0; i < samplelen; i++)
		{
			value = 0;
			memcpy(&value, ptr1, 3);
			if (value & 0x800000) {
				// 如果是负数，将高 8 位全部置为 1
				value |= 0xFF000000;
			}
			*ptr2 = value * coeff;

			ptr1 -= 3;//24位整数 3个字节offset
			ptr2--;
		}
	}
	//32位整数转float
	void static Int32toFloat(int* src, int samplelen, float* dest)
	{
		IntToFloat(src, samplelen, dest, 1.0f);
	}

	//float转16位整数
	void static FloattoInt16(float* src, int samplelen, short* dest)
	{
		FloatToInt(src, samplelen, dest);
	}

	//float转字节流
	void static FloattoInt24Byte(float* src, int samplelen, char* dest)
	{
		const int q_max = 8388607;   //量化为最大值上下对称
		const int q_min = -8388608;

		float* ptr1 = src + samplelen - 1 ;
		char* ptr2 = dest + (samplelen - 1) * 3;//-3 偏移
		int value = 0;
		for (int i = 0; i < samplelen; i++)
		{
			if (*ptr1 >= 1.0f)
			{
				value = q_max;
			}
			else if (*ptr1 <= -1.0f)
			{
				value = q_min;
			}
			else
			{
				value = static_cast<int>(std::round((*ptr1) * q_max));
			}

			ptr2[2] = value >> 16;
			ptr2[1] = value >> 8;
			ptr2[0] = value;

			ptr1--;
			ptr2 -= 3;
		}

	}

	//float转32位整数
	void static FloattoInt32(float* src, int samplelen, int* dest)
	{
		FloatToInt(src, samplelen, dest);
	}
};