#pragma once
// 极简 expected (基于 Result1 的 code=0 成功模型): 只做基础返回值逻辑, 无 and_then/transform 等
// T 存值, 错误固定为 std::string(全库用expected的error都是string); code==0 表示成功
// C++23 下代码统一用 std::expected(base_config 切 exp_ns 到 ycore 兼容别名)
#include<string>
#include<stdexcept>
#include<utility>

namespace ycore {

//----- unexpected: 构造失败结果 (exp_ns::unexpected(...)) -----
class unexpected_type
{
public:
	explicit unexpected_type(std::string e) : _err(std::move(e)) {}

	const std::string& error() const& noexcept { return _err; }
	std::string& error() & noexcept { return _err; }
	std::string&& error() && noexcept { return std::move(_err); }

private:
	std::string _err;
};

inline unexpected_type unexpected(const std::string& e) { return unexpected_type(e); }
inline unexpected_type unexpected(std::string&& e) { return unexpected_type(std::move(e)); }


template<typename T>
class expected
{
	template<typename U>
	friend class expected;

public:
	//成功: 默认构造 T
	expected()
		: _code{0}, _err{}
	{
		new(&_value)T();
	}

	//成功: 由值构造
	expected(const T& v)
		: _code{0}, _err{}
	{
		new(&_value)T(v);
	}
	expected(T&& v)
		: _code{0}, _err{}
	{
		new(&_value)T(std::move(v));
	}

	//失败
	expected(const unexpected_type& u)
		: _code{-1}, _err(u.error())
	{
	}
	expected(unexpected_type&& u)
		: _code{-1}, _err(std::move(u.error()))
	{
	}

	//不同值类型间的转换(如 expected<ASIOCapture*> -> expected<ICapture*>)
	template<typename U>
	expected(expected<U>&& other)
		: _code{other._code}, _err{std::move(other._err)}
	{
		if (_code == 0)
		{
			new(&_value)T(std::move(other.value()));
		}
		other._code = -1;
	}

	expected(expected&& other)
		: _code{other._code}, _err{std::move(other._err)}
	{
		if (_code == 0)
		{
			new(&_value)T(std::move(other._value));
		}
		other._code = -1;
	}

	expected& operator=(expected&& other)
	{
		if (this != &other)
		{
			if (_code == 0)
			{
				_value.~T();
			}
			_code = other._code;
			_err = std::move(other._err);
			if (_code == 0)
			{
				new(&_value)T(std::move(other._value));
			}
			other._code = -1;
		}
		return *this;
	}

	expected(const expected&) = delete;
	expected& operator=(const expected&) = delete;

	~expected()
	{
		if (_code == 0)
		{
			_value.~T();
		}
	}

	//结果检查: code==0 成功
	explicit operator bool() const noexcept { return _code == 0; }

	//取值, 失败时抛异常
	T& value() { return value_(); }
	const T& value() const { return value_(); }

	//解引用(与 std::expected 一致)
	T& operator*() { return value_(); }
	const T& operator*() const { return value_(); }
	T* operator->() { return &value_(); }
	const T* operator->() const { return &value_(); }

	//错误信息
	const std::string& error() const noexcept { return _err; }
	std::string& error() noexcept { return _err; }

	//----- 原 Result1 接口 -----
	int getCode() const noexcept { return _code; }
	const std::string& getError() const noexcept { return _err; }

	void setValue(T& value)
	{
		if (_code == 0)
		{
			_value.~T();
		}
		new(&_value)T(std::move(value));
		_code = 0;
		_err.clear();
	}

	void setFail(const std::string& err)
	{
		if (_code == 0)
		{
			_value.~T();
		}
		_err = std::move(err);
		_code = -1;
	}

	void setFail(int code, const std::string& err)
	{
		if (code == 0)
		{
			throw std::runtime_error("setFail code 不能为0");
		}
		if (_code == 0)
		{
			_value.~T();
		}
		_code = code;
		_err = std::move(err);
	}

private:
	T& value_()
	{
		if (_code != 0)
		{
			throw std::runtime_error("没有值");
		}
		return _value;
	}
	const T& value_() const
	{
		if (_code != 0)
		{
			throw std::runtime_error("没有值");
		}
		return _value;
	}

	int _code;
	std::string _err;
	union
	{
		T _value;
		char _dummy;  //占位符，保证union至少有一个成员
	};
};


//void 特化
template<>
class expected<void>
{
public:
	expected() : _code{0}, _err{} {}

	expected(const expected&) = delete;
	expected& operator=(const expected&) = delete;

	expected(expected&& other) noexcept
		: _code{other._code}, _err{std::move(other._err)}
	{
		other._code = -1;
	}

	expected& operator=(expected&& other) noexcept
	{
		if (this != &other)
		{
			_code = other._code;
			_err = std::move(other._err);
			other._code = -1;
		}
		return *this;
	}

	expected(const unexpected_type& u)
		: _code{-1}, _err(u.error())
	{
	}
	expected(unexpected_type&& u)
		: _code{-1}, _err(std::move(u.error()))
	{
	}

	explicit operator bool() const noexcept { return _code == 0; }
	const std::string& error() const noexcept { return _err; }
	std::string& error() noexcept { return _err; }

	int getCode() const noexcept { return _code; }
	const std::string& getError() const noexcept { return _err; }

private:
	int _code;
	std::string _err;
};

} // namespace ycore
