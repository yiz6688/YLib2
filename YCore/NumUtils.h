#pragma once

//计算下一个2的幂次方
template<typename N>
N nextpow2(N n)
{
	if(n <= 1)
	{
		return 1;
	}
	
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;

	if constexpr(sizeof(n) >= 2) n |= n >> 8;
	if constexpr(sizeof(n) >= 4) n |= n >> 16;
	if constexpr(sizeof(n) >= 8) n |= n >> 32;

	return n + 1;
}