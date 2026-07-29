#pragma once
#include<expected>
#include<memory>
#include<string>



template<typename T, typename E=std::string>
using TResult = std::expected<T, E>;

template<typename T>
using TPtr = std::unique_ptr<T>;


template<typename T, typename E=std::string>
using TPResult = std::expected<TPtr<T>, E>;


template<typename T, typename... Args>
inline TPResult<T> make_ok(Args&&... args)
{
    return TPtr<T>(std::make_unique<T>(std::forward<Args>(args)...));
}

template<typename T, typename E=std::string>
inline TPResult<T> make_err(E&& err)
{
    return std::unexpected(std::forward<E>(err));
}