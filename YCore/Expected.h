#pragma once
#include<string>
#include<type_traits>
#include<utility>
#include<stdexcept>
#include<functional>

// C++17 版 expected(在 Result1 基础上改造): code=0 表示成功, T 存储返回值, E 存储错误
// 接口与 std::expected 基本一致: 构造(值/错误/转换)、has_value/bool、value()/error()、operator* ->、value_or
// 仅实现基本功能; 另提供 code() 访问 code=0 成功模型
namespace ycore {

//错误标记(对应 std::unexpected)
template<typename E>
class Unexpected
{
public:
    explicit Unexpected(const E& e) : _error(e) {}
    explicit Unexpected(E&& e) : _error(std::move(e)) {}
    const E& error() const noexcept { return this->_error; }
    E& error() noexcept { return this->_error; }
private:
    E _error;
};

//构造错误标记(对应 std::unexpected, 支持CTAD)
template<typename E>
Unexpected<std::decay_t<E>> unexpected(E&& e)
{
    return Unexpected<std::decay_t<E>>(std::forward<E>(e));
}

template<typename T, typename E = std::string>
class Expected
{
public:
    Expected() : _code(0), _error{} { ::new (static_cast<void*>(&_value)) T(); }
    Expected(const T& v) : _code(0), _error{} { ::new (static_cast<void*>(&_value)) T(v); }
    Expected(T&& v) : _code(0), _error{} { ::new (static_cast<void*>(&_value)) T(std::move(v)); }
    Expected(const Unexpected<E>& u) : _code(-1), _error(u.error()) {}
    Expected(Unexpected<E>&& u) : _code(-1), _error(std::move(u.error())) {}
    template<typename E2, typename = std::enable_if_t<std::is_convertible_v<E2, E>>>
    Expected(const Unexpected<E2>& u) : _code(-1), _error(u.error()) {}
    template<typename E2, typename = std::enable_if_t<std::is_convertible_v<E2, E>>>
    Expected(Unexpected<E2>&& u) : _code(-1), _error(std::move(u.error())) {}
    //转换构造(如 ASIOCapture* -> ICapture*)
    template<typename U, typename E2,
        typename = std::enable_if_t<std::is_convertible_v<U, T> && std::is_convertible_v<E2, E>>>
    Expected(const Expected<U, E2>& other)
    {
        if (other.has_value())
        {
            ::new (static_cast<void*>(&_value)) T(other.value());
            _code = 0;
            _error = E();
        }
        else
        {
            _code = -1;
            _error = other.error();
        }
    }
    template<typename U, typename E2,
        typename = std::enable_if_t<std::is_convertible_v<U, T> && std::is_convertible_v<E2, E>>>
    Expected(Expected<U, E2>&& other)
    {
        if (other.has_value())
        {
            ::new (static_cast<void*>(&_value)) T(std::move(other.value()));
            _code = 0;
            _error = E();
        }
        else
        {
            _code = -1;
            _error = std::move(other.error());
        }
    }

    Expected(const Expected& other)
    {
        if (other.has_value())
        {
            ::new (static_cast<void*>(&_value)) T(other.value());
            _code = 0;
            _error = E();
        }
        else
        {
            _code = -1;
            _error = other.error();
        }
    }
    Expected& operator=(const Expected& other)
    {
        if (this != &other)
        {
            reset();
            if (other.has_value())
            {
                ::new (static_cast<void*>(&_value)) T(other.value());
                _code = 0;
            }
            else
            {
                _code = -1;
                _error = other.error();
            }
        }
        return *this;
    }
    Expected(Expected&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        if (other.has_value())
        {
            ::new (static_cast<void*>(&_value)) T(std::move(other.value()));
            _code = 0;
            _error = E();
        }
        else
        {
            _code = -1;
            _error = std::move(other.error());
        }
        other.reset();
    }
    Expected& operator=(Expected&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        if (this != &other)
        {
            reset();
            if (other.has_value())
            {
                ::new (static_cast<void*>(&_value)) T(std::move(other.value()));
                _code = 0;
            }
            else
            {
                _code = -1;
                _error = std::move(other.error());
            }
            other.reset();
        }
        return *this;
    }

    ~Expected() { reset(); }

    bool has_value() const noexcept { return _code == 0; }
    explicit operator bool() const noexcept { return _code == 0; }

    T& value()
    {
        if (_code != 0)
        {
            throw std::runtime_error("bad expected access");
        }
        return _value;
    }
    const T& value() const
    {
        if (_code != 0)
        {
            throw std::runtime_error("bad expected access");
        }
        return _value;
    }
    T& operator*() noexcept { return _value; }
    const T& operator*() const noexcept { return _value; }
    T* operator->() noexcept { return &_value; }
    const T* operator->() const noexcept { return &_value; }

    const E& error() const noexcept { return _error; }
    E& error() noexcept { return _error; }
    //code=0 成功模型(与 Result1 一致)
    int code() const noexcept { return _code; }

    T value_or(const T& def) const { return (_code == 0) ? _value : def; }

    //链式调用: 成功时调用 f(value), 失败时透传错误(对应 std::expected::and_then)
    template<typename F>
    auto and_then(F&& f)
    {
        using R = std::invoke_result_t<F, T&>;
        if (has_value())
        {
            return std::invoke(std::forward<F>(f), _value);
        }
        return R(unexpected(_error));
    }

    //链式调用: 失败时调用 f(error) 处理, 成功时透传值(对应 std::expected::or_else)
    template<typename F>
    auto or_else(F&& f)
    {
        using R = std::invoke_result_t<F, E&>;
        if (!has_value())
        {
            return std::invoke(std::forward<F>(f), _error);
        }
        return R(_value);
    }

private:
    void reset()
    {
        if (_code == 0)
        {
            _value.~T();
        }
        _code = -1;
        _error = E();
    }

private:
    union
    {
        T _value;
        char _dummy;
    };
    int _code;
    E _error;
};

//void 特化
template<typename E>
class Expected<void, E>
{
public:
    Expected() : _code(0), _error{} {}
    Expected(const Unexpected<E>& u) : _code(-1), _error(u.error()) {}
    Expected(Unexpected<E>&& u) : _code(-1), _error(std::move(u.error())) {}
    template<typename E2, typename = std::enable_if_t<std::is_convertible_v<E2, E>>>
    Expected(const Unexpected<E2>& u) : _code(-1), _error(u.error()) {}
    template<typename E2, typename = std::enable_if_t<std::is_convertible_v<E2, E>>>
    Expected(Unexpected<E2>&& u) : _code(-1), _error(std::move(u.error())) {}

    Expected(const Expected&) = default;
    Expected& operator=(const Expected&) = default;
    Expected(Expected&&) = default;
    Expected& operator=(Expected&&) = default;

    bool has_value() const noexcept { return _code == 0; }
    explicit operator bool() const noexcept { return _code == 0; }

    void value() const
    {
        if (_code != 0)
        {
            throw std::runtime_error("bad expected access");
        }
    }

    const E& error() const noexcept { return _error; }
    E& error() noexcept { return _error; }
    int code() const noexcept { return _code; }

private:
    int _code;
    E _error;
};

//小写别名, 与 std::expected 命名一致(exp_ns::expected = ycore::expected)
template<typename T, typename E = std::string>
using expected = Expected<T, E>;

} // namespace ycore
