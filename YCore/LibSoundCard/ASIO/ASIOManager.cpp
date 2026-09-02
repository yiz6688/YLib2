/*
该类用于获取pc上的所有板卡
*/

#include"ASIOManager.h"
#include"ASIODriver.h"
#include<Windows.h>

using namespace std;


constexpr char ASIO_PATH[] = { "software\\asio" };
constexpr char COM_CLSID[] = { "clsid" };
constexpr int MAXPATHLEN = 512;
constexpr int MAXDRVNAMELEN = 128;


static std::vector<ASIOInfo> getASIOInfos()
{
	std::vector<ASIOInfo> asioInfos;
	HKEY hkEnum = nullptr;
	HKEY hkSub = 0;
	long cr;
	DWORD index = 0;
	DWORD datatype = REG_SZ, datasize = 256;
	char keyname[MAXPATHLEN];
	char	databuf[MAXPATHLEN];
	wchar_t	wData[MAXDRVNAMELEN];
	CLSID	clsid;

	cr = RegOpenKeyA(HKEY_LOCAL_MACHINE, ASIO_PATH, &hkEnum);


	void* handle = nullptr;

	while (cr == ERROR_SUCCESS)
	{
		cr = RegEnumKeyA(hkEnum, index++, keyname, MAXDRVNAMELEN);
		if (cr == ERROR_SUCCESS) {

			cr = RegOpenKeyExA(hkEnum, keyname, 0, KEY_READ, &hkSub);
			if (cr == ERROR_SUCCESS) {

				cr = RegQueryValueExA(hkSub, COM_CLSID, 0, &datatype, (LPBYTE)databuf, &datasize);
				if (cr == ERROR_SUCCESS)
				{
					MultiByteToWideChar(CP_ACP, 0, (LPCSTR)databuf, -1, wData, 100);
					cr = CLSIDFromString(wData, (LPCLSID)&clsid);
					if (cr != S_OK) {
						continue;
					}
					//添加进去
					asioInfos.push_back({ clsid, keyname });
				}
				RegCloseKey(hkSub);
			}
		}
	}

	if (hkEnum != nullptr)
	{
		RegCloseKey(hkEnum);
	}
	return asioInfos;
}





ASIOManager::ASIOManager()
{
	this->asioInfos = getASIOInfos();
}

ASIOManager::~ASIOManager()
{}

long ASIOManager::getDeviceNum()
{
	return this->asioInfos.size();
}

ASIOInfo* ASIOManager::getDeviceInfo(unsigned index)
{
	if (index >= this->asioInfos.size() || index < 0)
	{
		return nullptr;
	}
	return &this->asioInfos[index];
}
ASIOInfo* ASIOManager::operator[](unsigned index)
{
	if (index >= this->asioInfos.size() || index < 0)
	{
		return nullptr;
	}
	return &this->asioInfos[index];
}

TPResult<ASIOClient> ASIOManager::createClient(unsigned index)
{
	ASIOInfo* info = this->getDeviceInfo(index);

	auto result = ASIODriver::createDriver(info->clsid);
	if (result)
	{
		ASIODriver* driver = result.value();
		return std::make_unique<ASIOClient>(driver);
	}
	else
	{
		return std::unexpected(result.error());
	}
}


