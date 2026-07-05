#pragma once
#include"../YCore/Stream.h"
#include"../YCore/FileStream.h"
#include"../YCore/MemoryStream.h"




class StreamTest
{


public:
	static void fileStreamWrite();

	static void fileStreamRead();

	static void fileStreamSeek();

	//----

	static void memoryStreamWrite();

};

