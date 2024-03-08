#include "DeviceEnumerator.h"

std::map<int, Device> DeviceEnumerator::getVideoDevicesMap(int modeltype) {
    return getDevicesMap(CLSID_VideoInputDeviceCategory,modeltype);
}

std::map<int, Device> DeviceEnumerator::getAudioDevicesMap() {
    return getDevicesMap(CLSID_AudioInputDeviceCategory,0);
}


// Returns a map of id and devices that can be used
std::map<int, Device> DeviceEnumerator::getDevicesMap(const GUID deviceClass, int model_type)
{
	std::map<int, Device> deviceMap;

	HRESULT hr = CoInitialize(nullptr);
	if (FAILED(hr)) {
		return deviceMap; // Empty deviceMap as an error
	}

    // Create the System Device Enumerator
    ICreateDevEnum *pDevEnum;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

	// If succeeded, create an enumerator for the category
	IEnumMoniker *pEnum = NULL;
	if (SUCCEEDED(hr)) {
		hr = pDevEnum->CreateClassEnumerator(deviceClass, &pEnum, 0);
		if (hr == S_FALSE) {
			hr = VFW_E_NOT_FOUND;
		}
		pDevEnum->Release();
	}

	// Now we check if the enumerator creation succeeded
	int deviceId = -1;
	if (SUCCEEDED(hr)) {
		// Fill the map with id and friendly device name
        IMoniker *pMoniker = NULL;
        IBaseFilter* pCaptureFilter = NULL;
        IBaseFilter* pCaptureFilter_cam = NULL;
        IAMVideoProcAmp *pProcAmp = 0;
        IAMCameraControl *pProcCom = 0;

        long Min, Max, Step, Default, Flags, Val;

		while (pEnum->Next(1, &pMoniker, NULL) == S_OK) {

			IPropertyBag *pPropBag;
			HRESULT hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
			if (FAILED(hr)) {
				pMoniker->Release();
				continue;
			}

			// Create variant to hold data
			VARIANT var;
			VariantInit(&var);

			std::string deviceName;
			std::string devicePath;

			// Read FriendlyName or Description
			hr = pPropBag->Read(L"Description", &var, 0); // Read description
			if (FAILED(hr)) {
				// If description fails, try with the friendly name
				hr = pPropBag->Read(L"FriendlyName", &var, 0);
			}
			// If still fails, continue with next device
			if (FAILED(hr)) {
				VariantClear(&var);
				continue;
			}
			// Convert to string
			else {
				deviceName = ConvertBSTRToMBS(var.bstrVal);
			}


            if(deviceName=="Adesso CyberTrack 6S "){

                 hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void**)&pCaptureFilter);

                 hr = pCaptureFilter ->QueryInterface(IID_IAMVideoProcAmp, (void**)&pProcAmp);

                 hr = pProcAmp->GetRange(VideoProcAmp_WhiteBalance, &Min, &Max, &Step,
                        &Default, &Flags);


                 if (SUCCEEDED(hr))
                    {
                        // Get the current value.
                        hr = pProcAmp->Get(VideoProcAmp_WhiteBalance, &Val, &Flags);

                        if(model_type==1){
                            //Gen2V1
                            Val=-23;
                            Flags=2;
                            hr = pProcAmp->Set(VideoProcAmp_Brightness, Val, Flags);
                            Val=-38;
                            Flags=2;
                            hr = pProcAmp->Set(VideoProcAmp_Contrast, Val, Flags);
                            Val=-64;
                            Flags=2;
                            hr = pProcAmp->Set(VideoProcAmp_Saturation, Val, Flags);
                            Val=-3;
                            Flags=2;
                            hr = pProcAmp->Set(VideoProcAmp_Sharpness, Val, Flags);
                            Val=1;
                            Flags=2;
                            hr = pProcAmp->Set(VideoProcAmp_BacklightCompensation, Val, Flags);
                            Val=4182;
                            Flags=2;
                            hr = pProcAmp->Set(VideoProcAmp_WhiteBalance, Val, Flags);
                        }
                    }

            }
            else if(deviceName=="MicrosoftR LifeCam Studio(TM)"){

                hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void**)&pCaptureFilter);
                hr = pCaptureFilter ->QueryInterface(IID_IAMVideoProcAmp, (void**)&pProcAmp);
                hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void**)&pCaptureFilter_cam);
                hr = pCaptureFilter_cam ->QueryInterface(IID_IAMCameraControl, (void**)&pProcCom);

                if(model_type==1){
                    // Gen2V1
                    if (SUCCEEDED(hr))
                       {
                        long ma,mi,ste,de;
                           // Get the current value.

                           hr = pProcCom->GetRange(CameraControl_Exposure, &ma, &mi, &ste,                                                   &de, &Flags);                      

                           Val = 23;
                           Flags = 2;
                           hr = pProcCom->Get(CameraControl_Focus, &Val, &Flags);
                           Val=47;
                           Flags=2;
                           hr = pProcAmp->Set(VideoProcAmp_Brightness, Val, Flags);
                           Val=5;
                           Flags=2;
                           hr = pProcAmp->Set(VideoProcAmp_Contrast, Val, Flags);
                           Val=200;
                           Flags=2;
                           hr = pProcAmp->Set(VideoProcAmp_Saturation, Val, Flags);
                           Val=49;
                           Flags=2;
                           hr = pProcAmp->Set(VideoProcAmp_Sharpness, Val, Flags);
                           Val=0;
                           Flags=2;
                           hr = pProcAmp->Set(VideoProcAmp_BacklightCompensation, Val, Flags);
                           Val=5100;
                           Flags=2;
                           hr = pProcAmp->Set(VideoProcAmp_WhiteBalance, Val, Flags);

                       }
                }


            }else if(deviceName=="Etron"){

                hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void**)&pCaptureFilter);
                hr = pCaptureFilter ->QueryInterface(IID_IAMVideoProcAmp, (void**)&pProcAmp);
                hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void**)&pCaptureFilter_cam);
                hr = pCaptureFilter_cam ->QueryInterface(IID_IAMCameraControl, (void**)&pProcCom);


                hr = pProcAmp->GetRange(VideoProcAmp_WhiteBalance, &Min, &Max, &Step,
                       &Default, &Flags);

                if(model_type==1){
                    // M model
                    if (SUCCEEDED(hr))
                       {
                           // Get the current value.
                           hr = pProcAmp->Get(VideoProcAmp_WhiteBalance, &Val, &Flags);
                           hr = pProcCom->Get(CameraControl_Exposure, &Val, &Flags);

                           printf("white_balance_flag:%ld",Flags);
                           printf("white_balance:%ld",Val);

                           Val=47;
                           Flags=2;
                           hr = pProcAmp->Set(VideoProcAmp_Brightness, Val, Flags);
                           Val=5;
                           Flags=2;
                           hr = pProcAmp->Set(VideoProcAmp_Contrast, Val, Flags);
                           Val=200;
                           Flags=2;
                           hr = pProcAmp->Set(VideoProcAmp_Saturation, Val, Flags);
                           Val=49;
                           Flags=2;
                           hr = pProcAmp->Set(VideoProcAmp_Sharpness, Val, Flags);
                           Val=0;
                           Flags=2;
                           hr = pProcAmp->Set(VideoProcAmp_BacklightCompensation, Val, Flags);
                           Val=5100;
                           Flags=2;
                           hr = pProcAmp->Set(VideoProcAmp_WhiteBalance, Val, Flags);
                       }
                }


            }


			VariantClear(&var); // We clean the variable in order to read the next value

            // try to read the DevicePath
			hr = pPropBag->Read(L"DevicePath", &var, 0);
			if (FAILED(hr)) {
				VariantClear(&var);
				continue; // If it fails we continue with next device
			}
			else {
				devicePath = ConvertBSTRToMBS(var.bstrVal);
			}

			// We populate the map
			deviceId++;
			Device currentDevice;
			currentDevice.id = deviceId;
			currentDevice.deviceName = deviceName;
			currentDevice.devicePath = devicePath;
			deviceMap[deviceId] = currentDevice;

		}
        if(deviceId!=0){
            pCaptureFilter->Release();
            pProcAmp->Release();
        }

        pEnum->Release();
	}


	CoUninitialize();
	return deviceMap;
}

/*
This two methods were taken from
https://stackoverflow.com/questions/6284524/bstr-to-stdstring-stdwstring-and-vice-versa
*/

std::string DeviceEnumerator::ConvertBSTRToMBS(BSTR bstr)
{
	int wslen = ::SysStringLen(bstr);
	return ConvertWCSToMBS((wchar_t*)bstr, wslen);
}

std::string DeviceEnumerator::ConvertWCSToMBS(const wchar_t* pstr, long wslen)
{
	int len = ::WideCharToMultiByte(CP_ACP, 0, pstr, wslen, NULL, 0, NULL, NULL);

	std::string dblstr(len, '\0');
	len = ::WideCharToMultiByte(CP_ACP, 0 /* no flags */,
		pstr, wslen /* not necessary NULL-terminated */,
		&dblstr[0], len,
		NULL, NULL /* no default char */);

	return dblstr;
}
