#include "Result.h"

Result::Result()
	:Result(0, "")
{}

Result::Result(int code, const std::string& error)
	:code(code), error(error)
{
	this->writeERROR(error);
}

Result::Result(Result&& other) noexcept
	: code(other.code), error(std::move(other.error))
{
	this->logLst = std::move(other.logLst);
}

Result& Result::operator=(Result&& other) noexcept
{
	if (this != &other)
	{
		this->code = other.code;
		this->error = std::move(other.error);
		this->logLst = std::move(other.logLst);
	}
	return *this;
}

void Result::setFail(int code, const std::string& error)
{
	this->code = code;
	this->error = error;
	this->writeERROR(error);
}

void Result::setFail(const std::string& error)
{
	this->setFail(-1, error);
}

void Result::setPass()
{
	this->code = 0;
	this->error.clear();
}

void Result::applyAppend(const Result& result)
{
	if (this == &result)
	{
		return;
	}
	this->code = result.code;
	this->error = result.error;
	this->append(result);
}

void Result::applyPrepend(const Result& result)
{
	if (this == &result)
	{
		return;
	}
	this->code = result.code;
	this->error = result.error;
	this->prepend(result);
}
