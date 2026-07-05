#pragma once
#include<cmath>
#include<limits>
#include<random>

class SampleConv
{




public:
	template<typename T>
	static void IntToFloat(T* src, int samplelen, float* dest, float scale = 1.0f)
	{
		constexpr int q_min = (std::numeric_limits<T>::min)();   //量化为最小值上下对称
		//constexpr int q_max = (std::numeric_limits<T>::max)();	
		const float coeff = -1.0f * scale / q_min ;  //用最小值也就是大的值进行转浮点数
		T* ptr1 = src + samplelen - 1;
		float* ptr2 = dest + samplelen - 1;

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
		const int coeff = q_max;  //用最大值也就是大的值进行转浮点数


		//static std::random_device rd;
		//static std::mt19937 gen(rd());
		// 生成 -1.0 到 1.0 之间的均匀分布随机数
		//static std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

		float* ptr1 = src + samplelen - 1;
		T* ptr2 = dest + samplelen - 1;
		for (int i = 0; i < samplelen; i++)
		{
			//float noise1 = dis(gen);
			//float noise2 = dis(gen);
			//auto value = static_cast<int>(std::round((*src) * coeff * scale + noise1 + noise2));
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




public:
	//16位整数转float
	static void Int16toFloat(short* src, int samplelen, float* dest)
	{
		IntToFloat(src, samplelen, dest);
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
		IntToFloat(src, samplelen, dest);
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