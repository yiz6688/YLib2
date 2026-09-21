#pragma once
#include"base_config.hpp"
#include<memory>
#include<string>



template<typename T>
using TResult = exp_ns::expected<T>;

template<typename T>
using TPtr = std::unique_ptr<T>;


template<typename T>
using TPResult = exp_ns::expected<TPtr<T>>;


template<typename T, typename... Args>
inline TPResult<T> make_ok(Args&&... args)
{
    return TPtr<T>(std::make_unique<T>(std::forward<Args>(args)...));
}

template<typename T>
inline TPResult<T> make_err(const std::string& err)
{
    return exp_ns::unexpected(err);
}
