#pragma once
#include"LogBuffer.h"
#include<string>
#include<stdexcept>
#include<utility>



template<typename T>
class Result1
{

public:
	Result1()
		:code{ 0 }, error{}
	{
		new(&this->_value)T(); //调用默认构造
	}

	Result1(int _code, const std::string& _err)
		:code{ _code }, error{_err}
	{
		if(this->code == 0)
		{
			new(&this->_value)T();
			this->error = "";
		}
	}

	Result1(Result1&& obj)
	{
		this->code = obj.code;
		this->error = std::move(obj.error);
		if(this->code == 0)
		{
			this->_value = std::move(obj._value);
		}
		
		obj.code = -1;
	}


	Result1& operator=(Result1&& obj)
	{
		if(this == &obj)
		{
			return *this;
		}

		this->code = obj.code;
		this->error = std::move(obj.error);
		if(this->code == 0)
		{
			this->_value = std::move(obj._value);
		}
		obj.code = -1;

	}



	~Result1()
	{
		if(this->code == 0)
		{
			this->_value.~T();  //调用析构
			this->code = -1;
		}
	}

public:
	void setValue(T& value)
	{
		if(this->code == 0)
		{
			this->_value.~T();
		}

		this->_value = std::move(value);
		this->code = 0;
		this->error = "";
	}

	void setFail(const std::string& err)
	{
		if(this->code == 0)
		{
			this->_value.~T();
		}
		this->error = std::move(err);
		this->code = -1;
	}

	void setFail(int _code, const std::string& err)
	{
		if(_code == 0)
		{
			throw std::runtime_error("setFail code 不能为0");
		}

		if(this->code == _code)
		{
			this->error = std::move(err);
			return;
		}

		if(this->code == 0)
		{
			this->_value.~T();
		}

		this->code = _code;
		this->error = std::move(err);

	}


public:

	int getCode()
	{
		return this->code;
	}

	const std::string& getError()
	{
		return this->error;
	}

	T& value()
	{
		if (this->code == 0)
		{
			return this->_value;
		}
		else
		{
			throw std::runtime_error("没有值");
		}
	}


protected:
	
	int code;
	std::string error;
	union
	{
		T _value;
		char _dummy;  //占位符，保证union至少有一个成员
	};
	bool hasValue = false;  //标记是否有值
};








class Result : public LogBuffer
{


public:
	Result();

	Result(int code, const std::string& error);

	Result(const Result&) = default;

	Result(Result&& other) noexcept;

	Result& operator=(const Result&) = default;

	Result& operator=(Result&& other) noexcept;


	void setFail(int code, const std::string& error);

	void setFail(const std::string& error);

	void setPass();


	void applyAppend(const Result& result);
	void applyPrepend(const Result& result);

public:
	int code;
	std::string error;
};


template<typename T>
class ResultWith : public Result
{

public:

    ResultWith()
		: Result(0, "")
	{

    }

	ResultWith(const T& value)
		: Result(0, ""), _value(value), hasValue(true)
	{

	}

	ResultWith(int code, const std::string& error, const T& value)
		:Result(code, error), _value(value)
	{
	}
	ResultWith(int code, const std::string& error, T&& value)
		:Result(code, error), _value(std::move(value))
	{
	}
	ResultWith(const ResultWith&) = default;
	ResultWith(ResultWith&& other) noexcept
		: Result(std::move(other)), _value(std::move(other._value))
	{
	}
	ResultWith& operator=(const ResultWith&) = default;
	ResultWith& operator=(ResultWith&& other) noexcept
	{
		if (this != &other)
		{
			Result::operator=(std::move(static_cast<Result&&>(other)));
			if (this->hasValue)
			{
               this->_value.~T();
			}
			new(&this->_value) T(std::move(other._value)); //移动构建
			this->hasValue = true;
		}
		return *this;
	}



	//void operator=(const T& value)
	//{

	//}

	void operator=(T&& value)
	{
		
		if (this->hasValue)
		{
			this->_value.~T();
			this->hasValue = false;
		}
		
		this->code = 0;
		this->error.clear();
		new(&this->_value) T(std::move(value)); //移动构建
		this->hasValue = true;
		
	}



	T& value()
	{
		if (this->hasValue)
		{
			return this->_value;
		}
		else
		{
			//throw std::runtime_error("没有值");
		}
		
	}





protected:
	union
	{
		T _value;
		char _dummy;  //占位符，保证union至少有一个成员
	};
	bool hasValue = false;  //标记是否有值
};