#pragma once
// 公共配置: C++23(支持 <expected>/<print>) 用标准库; C++17/20 用 fmt/gsl + 内置 Expected
// 代码统一使用宏: fmt_ns(格式化/打印), span_ns(span), exp_ns(expected)
// 切换 C++ 版本: 修改根目录 CmakeLists.txt 的 YCORE_USE_CPP17 开关即可

//MSVC 默认定义 min/max 宏, 会破坏 std::min/std::max 与 (min)()/(max)() 等用法
#ifdef _MSC_VER
#  define NOMINMAX
#endif

#include<string>

// 语言标准检测: 用 _MSVC_LANG(MSVC 始终准确) 或 __cplusplus(GCC/Clang 准确)
// 注意: 不能用 __has_include 判断, GCC 在 -std=c++17 下也能看到 <expected> 头文件存在
#if defined(_MSVC_LANG)
#  define YCORE_CPLUSPLUS _MSVC_LANG
#else
#  define YCORE_CPLUSPLUS __cplusplus
#endif

#if YCORE_CPLUSPLUS >= 202302L
#  define YCORE_USE_STD23 1
#endif

#if defined(YCORE_USE_STD23)
//---------- C++23: 使用标准库 ----------
#  include <expected>
#  include <print>
#  include <format>
#  include <span>
#  include <bit>
#  define fmt_ns  std
#  define span_ns std
#  define exp_ns  std
#else
//---------- C++17/20: 使用 fmt / gsl / 内置 Expected ----------
#  include <fmt/format.h>
#  include <fmt/chrono.h>
#  include <gsl/span>
#  include "Expected.h"
#  define fmt_ns  fmt
#  define span_ns gsl
#  define exp_ns  ycore
#endif


namespace ycore {
//std::bit_ceil 的 C++17 兼容实现(C++23 下与标准库一致)
template<typename T>
constexpr T bit_ceil(T value)
{
    if (value <= 1)
    {
        return 1;
    }
    T v = value - 1;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    if constexpr (sizeof(T) >= 2) v |= v >> 8;
    if constexpr (sizeof(T) >= 4) v |= v >> 16;
    if constexpr (sizeof(T) >= 8) v |= v >> 32;
    return v + 1;
}

//π 常量(替代 C++20 的 std::numbers::pi)
inline constexpr double pi = 3.14159265358979323846;
}
