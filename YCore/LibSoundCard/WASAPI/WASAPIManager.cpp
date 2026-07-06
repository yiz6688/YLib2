#include"WASAPIManager.h"

#include<initguid.h>
#include<mmdeviceapi.h>
#include<string>
#include <endpointvolume.h>
#include<Audioclient.h>
#include<Functiondiscoverykeys_devpkey.h>
#include"../../Encoding.h"

std::vector<EndPointInfo> WASAPIManager::getEndPoints(EDataFlow eDataFlow, DWORD dwMask)
{
	IMMDeviceCollection* pCollection;
	IMMDeviceEnumerator* pEnumerator;
	unsigned nDevices;

	std::vector<EndPointInfo> lst;


	//STA方式初始化
	auto hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(hr))
	{
		throw "COM Initialize 失败";
	}

	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);

	if (FAILED(hr))
	{
		CoUninitialize();
		throw "获取Enumerator失败";
	}

	//dwMask = 0;//设备的状态， 激活的 未激活的这些

	if (dwMask > 0xF || dwMask == 0)
	{
		throw "非法的dwMask";
	}

	// DEVICE_STATE_ACTIVE
	hr = pEnumerator->EnumAudioEndpoints(eDataFlow, dwMask, &pCollection);

	if (FAILED(hr))
	{
		CoUninitialize();
		throw "获取Collection失败";
	}

	hr = pCollection->GetCount(&nDevices);

	if (FAILED(hr))
	{
		CoUninitialize();
		throw "GetCount失败";
	}

	lst.reserve(nDevices);

	EndPointInfo info;

	IMMDevice* pDevice = nullptr;

	for (int index = 0; index < nDevices; index++)
	{
		info.index = index;
		auto hr = pCollection->Item(index, &pDevice);
		if (FAILED(hr))
		{
			throw  "get Device Fail";
		}

		wchar_t* pDevId;
		hr = pDevice->GetId(&pDevId);
		info.id = Encoding::UTF162UTF8(pDevId);  //转换为utf8字符串
		CoTaskMemFree(pDevId);

		IPropertyStore* pProperty = nullptr;
		pDevice->OpenPropertyStore(STGM_READ, &pProperty);

		PROPVARIANT varName;
		PropVariantInit(&varName);
		hr = pProperty->GetValue(PKEY_Device_FriendlyName, &varName);
		if (SUCCEEDED(hr) && varName.vt != VT_EMPTY)
		{
			info.frindlyName = Encoding::UTF162UTF8(varName.pwszVal);  //转换为utf8字符串
		}
		PropVariantClear(&varName);


		lst.push_back(info);
	}


	return lst;
}
