#include "StreamTest.h"
#include"../YCore/FileStream.h"
#include<print>

using namespace std;


string getStreamInfo(Stream& stream)
{
	auto lenResult = stream.getLength();
	if (!lenResult)
	{
		return "";
	}

	auto posResult = stream.getPosition();
	if(!posResult)
	{
		return "";
	}


	return std::format("len:{} pos:{}", lenResult.value(), posResult.value());
}



void StreamTest::fileStreamWrite()
{
	FileStream	fs(R"(D:\0\test.txt)", FileMode::OpenOrCreate, FileAccess::ReadWrite, FileShare::Read, 128);
	
	//auto rr =    fs.init(R"(D:\0\test.txt)", FileAccess::ReadWrite, FileShare::ReadWrite, FileMode::OpenOrCreate);
	//if (!rr)
	//{
	//	println("初始化失败: {}", rr.error());
	//	return;
	//}

	auto result = fs.getLength();
	if (result)
	{
		println("文件长度: {}", result.value());
	}
	else
	{
		println("获取文件长度失败: {}", result.error());
		return;
	}
	
	//string str = "Hello, FileStream!\n";
	for (int i = 0; i < 11; i++)
	{
		 result = fs.write(std::format("{} {}",i , "Hello, FileStream!\n"));
		if(result)
		{
			int wSize = result.value();
			auto info = getStreamInfo(fs);
			println("{} 写入了 {} 字节 {}",i, wSize, info);
		}
		else
		{
			println("写入失败: {}", result.error());
			return;
		}
	}


	/*result = fs.seek(-100, SeekOrigin::Current);
	if (result)
	{
		int pos = result.value();
		println("当前文件位置: {}", pos);

	}
	else
	{
		println("设置文件位置失败: {}", result.error());
		return;
	}*/


	return;
	char buffer[43];
	buffer[42] = '\0';
	result = fs.read(buffer, 42);
	if(!result)
	{
		println("读取失败: {}", result.error());
		return;
	}
	else
	{
		buffer[result.value()] = '\0';
		println("读取长度: {}\n 内容:{}\n   {}", result.value(), buffer, getStreamInfo(fs));
	}


}

void StreamTest::fileStreamRead()
{
	FileStream	fs(R"(D:\0\test.txt)", FileMode::OpenOrCreate, FileAccess::ReadWrite, FileShare::Read, 1024);
	auto result = fs.getLength();
	if (result)
	{
		println("文件长度: {}", result.value());
	}
	else
	{
		println("获取文件长度失败: {}", result.error());
		return;
	}


	char buffer[17];
	buffer[16] = '\0';

	while (true)
	{
		result = fs.read(buffer, 16);
		if (!result)
		{
			println("{}", result.error());
			return;
		}
		if (result.value() == 0)
		{
			println("读取结束: {}", result.value());
			return;
		}
		buffer[result.value()] = '\0';
		//println("读取长度: {}\n 内容:{}\n   {}", result.value(), buffer, getStreamInfo(fs));
		print("{}", buffer);
	}


}

void StreamTest::fileStreamSeek()
{
	FileStream	fs(R"(D:\0\test.txt)", FileMode::OpenOrCreate, FileAccess::ReadWrite, FileShare::Read, 24);
	auto result = fs.getLength();
	if (result)
	{
		println("文件长度: {}", result.value());
	}
	else
	{
		println("获取文件长度失败: {}", result.error());
		return;
	}

	char buffer[17];
	buffer[16] = '\0';
	result = fs.read(buffer, 16);
	if (!result)
	{
		println("{}", result.error());
		return;
	}
	else
	{
		buffer[result.value()] = '\0';
		println("读取长度: {}\n 内容:{}\n   {}", result.value(), buffer, getStreamInfo(fs));
	}

	
	result = fs.seek(-2, SeekOrigin::Current);
	if (!result)
	{
		println("{}", result.error());
		return;
	}
	else
	{
		println("指针位置: {}", result.value());
	}

	result = fs.read(buffer, 16);
	if (!result)
	{
		println("{}", result.error());
		return;
	}
	else
	{
		buffer[result.value()] = '\0';
		println("读取长度: {}\n 内容:{}\n   {}", result.value(), buffer, getStreamInfo(fs));
	}

	result = fs.read(buffer, 16);
	if (!result)
	{
		println("{}", result.error());
		return;
	}
	else
	{
		buffer[result.value()] = '\0';
		println("读取长度: {}\n 内容:{}\n   {}", result.value(), buffer, getStreamInfo(fs));
	}


}

void StreamTest::memoryStreamWrite()
{
	MemoryStream ms;



}
