#include "targetver.h"

#define _CRT_SECURE_NO_WARNINGS
// Windows Header Files:
#include <windows.h>
#include <strsafe.h>
#include <msiquery.h>
#include <Setupapi.h>
#include <shlwapi.h>
#include <cfgmgr32.h>
#include <newdev.h>
#include <Regstr.h>

char buf[2*MAX_PATH+10];
void LogMessage (MSIHANDLE hInstall, char *fmt,...)
{
	va_list arglist;
	va_start(arglist, fmt);
    wvsprintf (buf, fmt, arglist);
	va_end(arglist);
	PMSIHANDLE hrec = MsiCreateRecord (1);
	OutputDebugString (buf);
	MsiRecordSetStringA (hrec, 0, buf);
	MsiProcessMessage (hInstall, INSTALLMESSAGE_INFO, hrec);
}

void MsiMessageBox (MSIHANDLE hInstall, char *fmt,...)
{
	va_list arglist;
	va_start(arglist, fmt);
    wvsprintf (buf, fmt, arglist);
	va_end(arglist);
	PMSIHANDLE hrec = MsiCreateRecord (1);
	MsiRecordSetStringA (hrec, 0, buf);
	MsiProcessMessage (hInstall, INSTALLMESSAGE(INSTALLMESSAGE_USER|MB_OK), hrec);
}

UINT InstallDriver (MSIHANDLE hInstall, LPCSTR InfFilePath, LPCSTR devId)
{
    GUID ClassGUID;
    HDEVINFO DeviceInfoSet = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA DeviceInfoData;
    char ClassName[MAX_CLASS_NAME_LEN];
    char hwIdList[LINE_LEN+4];

	if (!SetupDiGetINFClass (InfFilePath, &ClassGUID, ClassName, sizeof ClassName, 0))
    {
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "InstallDriver: SetupDiGetINFClass failed: dwErr = %d", dwErr);
		return ERROR_INSTALL_FAILURE;
    }

    //
    // Create the container for the to-be-created Device Information Element.
    //
    DeviceInfoSet = SetupDiCreateDeviceInfoList (&ClassGUID,0);
    if(DeviceInfoSet == INVALID_HANDLE_VALUE)
    {
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "InstallDriver: SetupDiCreateDeviceInfoList failed: dwErr = %d", dwErr);
		return ERROR_INSTALL_FAILURE;
    }

    //
    // Now create the element.
    // Use the Class GUID and Name from the INF file.
    //
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    if (!SetupDiCreateDeviceInfo (DeviceInfoSet,
        ClassName,
        &ClassGUID,
        NULL,
        0,
        DICD_GENERATE_ID,
        &DeviceInfoData))
    {
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "InstallDriver: SetupDiCreateDeviceInfo failed: dwErr = %d", dwErr);
        SetupDiDestroyDeviceInfoList (DeviceInfoSet);
		return ERROR_INSTALL_FAILURE;
    }

    //
    // Add the HardwareID to the Device's HardwareID property.
    //
    ZeroMemory(hwIdList, sizeof(hwIdList));
    if (FAILED(StringCchCopy(hwIdList, LINE_LEN, devId)))
	{
		LogMessage (hInstall, "InstallDriver: StringCchCopy failed");
        SetupDiDestroyDeviceInfoList (DeviceInfoSet);
		return ERROR_INSTALL_FAILURE;
    }
    if(!SetupDiSetDeviceRegistryProperty(DeviceInfoSet,
        &DeviceInfoData,
        SPDRP_HARDWAREID,
        (LPBYTE)hwIdList,
        (lstrlen(hwIdList)+1+1)))
    {
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "InstallDriver: SetupDiSetDeviceRegistryProperty failed: dwErr = %d", dwErr);
        SetupDiDestroyDeviceInfoList (DeviceInfoSet);
		return ERROR_INSTALL_FAILURE;
    }

    //
    // Transform the registry element into an actual devnode
    // in the PnP HW tree.
    //
    if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE,
        DeviceInfoSet,
        &DeviceInfoData))
    {
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "InstallDriver: SetupDiCallClassInstaller failed: dwErr = %d", dwErr);
        SetupDiDestroyDeviceInfoList (DeviceInfoSet);
		return ERROR_INSTALL_FAILURE;
    }

    //
    // update the driver for the device we just created
    //
    SetupDiDestroyDeviceInfoList(DeviceInfoSet);

    if(!UpdateDriverForPlugAndPlayDevices (NULL, devId, InfFilePath, 0, NULL))
	{
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "InstallDriver: UpdateDriverForPlugAndPlayDevices failed: dwErr = %d", dwErr);
		return ERROR_INSTALL_FAILURE;
    }
	return ERROR_SUCCESS;
}

UINT __stdcall Install(MSIHANDLE hInstall)
{
	HRESULT hr = S_OK;
	UINT er = ERROR_SUCCESS;

	char CustomActionData[2*MAX_PATH+10];
	DWORD n = sizeof CustomActionData;
	er = MsiGetProperty (hInstall, "CustomActionData", CustomActionData, &n);
	if (er != ERROR_SUCCESS)
	{
		LogMessage (hInstall, "MsiGetProperty (CustomActionData) returned %d", er);
		return ERROR_INSTALL_FAILURE;
	}

	char *sc = strchr (CustomActionData, ';');
	if (sc == NULL)
	{
		LogMessage (hInstall, "Invalid property: %s", CustomActionData);
		return ERROR_INSTALL_FAILURE;
	}

	char *devId = sc+1;
	char *InfFilePath = CustomActionData;
	*sc = 0;

	er = InstallDriver (hInstall, InfFilePath, devId);
	return er;
}

BOOL GetServiceName (HDEVINFO hDevInfo, SP_DEVINFO_DATA &DeviceInfoData, LPSTR buffer, DWORD buffersize)
{
	DEVPROPTYPE PropertyType;
	BOOL b = SetupDiGetDeviceRegistryProperty(
		hDevInfo,
		&DeviceInfoData,
		SPDRP_SERVICE,
		&PropertyType,
		(PBYTE)buffer,
		buffersize,
		&buffersize);
	return b;
}

BOOL GetDriverInfoDetail (MSIHANDLE hInstall, HDEVINFO hDevInfo, SP_DEVINFO_DATA &DeviceInfoData, PSP_DRVINFO_DATA DriverInfo,
			PSP_DRVINFO_DETAIL_DATA *DriverDetail)
{
	DWORD reqSize;
	SP_DRVINFO_DETAIL_DATA DetailData;
	ZeroMemory (&DetailData, sizeof (SP_DRVINFO_DETAIL_DATA));
	DetailData.cbSize = sizeof DetailData;
	BOOL b = SetupDiGetDriverInfoDetail (hDevInfo, &DeviceInfoData, DriverInfo, &DetailData,
		DetailData.cbSize, &reqSize);
	if (b)
		return FALSE;
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "GetDriverInfoDetail: SetupDiGetDriverInfoDetail(1) failed: dwErr = %d\n", dwErr);
		return FALSE;
	}
	*DriverDetail = (PSP_DRVINFO_DETAIL_DATA)LocalAlloc (LPTR, reqSize);
	if (*DriverDetail == NULL)
	{
		LogMessage (hInstall, "GetDriverInfoDetail: LocalAlloc (%d) failed\n", reqSize);
		return FALSE;
	}
	(*DriverDetail)->cbSize = sizeof (SP_DRVINFO_DETAIL_DATA);
	b = SetupDiGetDriverInfoDetail (hDevInfo, &DeviceInfoData, DriverInfo, *DriverDetail, reqSize, NULL);
	if (!b)
	{
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "GetDriverInfoDetail: SetupDiGetDriverInfoDetail failed: dwErr = %d\n", dwErr);
		LocalFree (*DriverDetail);
		*DriverDetail = NULL;
		return FALSE;
	}
	return TRUE;
}

BOOL GetDriverInfPath (MSIHANDLE hInstall, HDEVINFO hDevInfo, SP_DEVINFO_DATA &DeviceInfoData, LPSTR InfPath)
{
	SP_DEVINSTALL_PARAMS params;
	ZeroMemory (&params, sizeof params);
	params.cbSize = sizeof (SP_DEVINSTALL_PARAMS);
	params.FlagsEx = DI_FLAGSEX_INSTALLEDDRIVER;
	BOOL b = SetupDiSetDeviceInstallParams (hDevInfo, &DeviceInfoData, &params);
	if (!b)
	{
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "GetDriverName: SetupDiSetDeviceInstallParams failed: dwErr = %d\n", dwErr);
		return FALSE;
	}
	b = SetupDiBuildDriverInfoList (hDevInfo, &DeviceInfoData, SPDIT_COMPATDRIVER);
	if (!b)
	{
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "GetDriverName: SetupDiBuildDriverInfoList failed: dwErr = %d\n", dwErr);
		return FALSE;
	}
	for (int j = 0;;j++)
	{
		SP_DRVINFO_DATA DriverInfo;
		ZeroMemory (&DriverInfo, sizeof DriverInfo);
		DriverInfo.cbSize = sizeof (SP_DRVINFO_DATA);
		b = SetupDiEnumDriverInfo (hDevInfo, &DeviceInfoData, SPDIT_COMPATDRIVER, j, &DriverInfo);
		if (!b)
		{
			DWORD dwErr = GetLastError();
			if (dwErr != ERROR_NO_MORE_ITEMS)
			{
				LogMessage (hInstall, "GetDriverName: SetupDiEnumDriverInfo (%d) failed: dwErr = %d\n", j, dwErr);
			}
			break;
		}
		PSP_DRVINFO_DETAIL_DATA DriverDetail;
		b = GetDriverInfoDetail (hInstall, hDevInfo, DeviceInfoData, &DriverInfo, &DriverDetail);
		if (!b)
		{
			DWORD dwErr = GetLastError();
			LogMessage (hInstall, "RemoveDevice: GetDriverInfoDetail failed: dwErr = %d\n", dwErr);
			return FALSE;
		}
		lstrcpy (InfPath, DriverDetail->InfFileName);
		LocalFree(DriverDetail);
	}
	SetupDiDestroyDriverInfoList (hDevInfo, &DeviceInfoData, SPDIT_COMPATDRIVER);
	return TRUE;
}

BOOL GetDriverImagePath (char *RegistryPath, char *buffer, DWORD buffersize)
{
	buffer[0] = 0;
	HKEY hKey;
	LONG dwErr = RegOpenKey (HKEY_LOCAL_MACHINE, RegistryPath, &hKey);
	if (dwErr != ERROR_SUCCESS)
	{
		SetLastError (dwErr);
		return FALSE;
	}
	dwErr = RegQueryValueEx (hKey, "ImagePath", NULL, NULL, (LPBYTE)buffer, &buffersize);
	RegCloseKey (hKey);
	if (dwErr != ERROR_SUCCESS)
	{
		SetLastError (dwErr);
		return FALSE;
	}
	return TRUE;
}

BOOL RemoveDevice (MSIHANDLE hInstall, HDEVINFO hDevInfo, SP_DEVINFO_DATA &DeviceInfoData)
{
	char serviceName[MAX_PATH];
	char InfPath[MAX_PATH];
	char RegistryPath[MAX_PATH];
	char ImagePath[MAX_PATH];
	BOOL b = GetServiceName (hDevInfo, DeviceInfoData, serviceName, MAX_PATH);
	if (!b)
	{
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "RemoveDevice: GetServiceName failed: dwErr = %d\n", dwErr);
		return FALSE;
	}
	wsprintf (RegistryPath, "SYSTEM\\CurrentControlSet\\Services\\%s", serviceName);
	b = GetDriverInfPath (hInstall, hDevInfo, DeviceInfoData, InfPath);
	if (!b)
	{
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "RemoveDevice: GetDriverInfPath failed: dwErr = %d\n", dwErr);
		return FALSE;
	}
	b = GetDriverImagePath (RegistryPath, ImagePath, MAX_PATH);
	if (!b)
	{
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "RemoveDevice: GetDriverImagePath failed: dwErr = %d\n", dwErr);
		return FALSE;
	}
	b = SetupDiCallClassInstaller (DIF_REMOVE, hDevInfo, &DeviceInfoData);
	if (!b)
	{
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "RemoveDevice: SetupDiCallClassInstaller (DIF_REMOVE) failed: dwErr = %d\n", dwErr);
		return FALSE;
	}
	DWORD dwErr = SHDeleteKey (HKEY_LOCAL_MACHINE, RegistryPath);
	if (dwErr != ERROR_SUCCESS)
	{
		LogMessage (hInstall, "RemoveDevice: SHDeleteKey (\"%s\") failed: dwErr = %d\n", RegistryPath, dwErr);
		return FALSE;
	}
	char *infFileName = strrchr (InfPath, '\\');infFileName++;
	b = SetupUninstallOEMInf (infFileName, SUOI_FORCEDELETE, NULL);
	if (!b)
	{
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "RemoveDevice: SetupUninstallOEMInf (\"%s\") failed: dwErr = %d\n", infFileName, dwErr);
		return FALSE;
	}
	b = DeleteFile (ImagePath);
	if (!b)
	{
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "RemoveDevice: DeleteFile (\"%s\") failed: dwErr = %d\n", ImagePath, dwErr);
		return FALSE;
	}
	return TRUE;
}

BOOL UninstallDriver (MSIHANDLE hInstall, char *DeviceId)
{
	HDEVINFO        hDevInfo;
	SP_DEVINFO_DATA DeviceInfoData;
	int j;
	BOOL bResult = FALSE;
	// scan all root-enumerated devices
	hDevInfo = SetupDiGetClassDevs (
		NULL,
		REGSTR_KEY_ROOTENUM,
		NULL,
		DIGCF_PRESENT|DIGCF_ALLCLASSES
		);
	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "SetupDiGetClassDevs failed, dwErr = %d", dwErr);
		return FALSE;
	}

	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	for (j = 0; SetupDiEnumDeviceInfo (hDevInfo, j, &DeviceInfoData); j++)
	{
		LPTSTR buffer = NULL;
		DWORD buffersize = 0;

		//
		// Call function with null to begin with,
		// then use the returned buffer size
		// to Alloc the buffer. Keep calling until
		// success or an unknown failure.
		//
	    DEVPROPTYPE PropertyType;
		while (!SetupDiGetDeviceRegistryProperty(
			hDevInfo,
			&DeviceInfoData,
			SPDRP_HARDWAREID,
			&PropertyType,
			(PBYTE)buffer,
			buffersize,
			&buffersize))
		{
			if (GetLastError() ==
				ERROR_INSUFFICIENT_BUFFER)
			{
				// Change the buffer size.
				if (buffer) LocalFree(buffer);
				buffer = (LPTSTR)LocalAlloc(LPTR,buffersize);
			}
			else
			{
				break;
			}
		}

		if (buffer)
		{
			if (_strcmpi (buffer, DeviceId) == 0)
			{
				bResult = RemoveDevice (hInstall, hDevInfo, DeviceInfoData);
			}
			LocalFree(buffer);
		}
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);
	return bResult;
}

UINT __stdcall Uninstall (MSIHANDLE hInstall)
{
	HRESULT hr = S_OK;
	UINT er = ERROR_SUCCESS;

	char CustomActionData[2*MAX_PATH+10];
	DWORD n = sizeof CustomActionData;
	er = MsiGetProperty (hInstall, "CustomActionData", CustomActionData, &n);
	if (er != ERROR_SUCCESS)
	{
		LogMessage (hInstall, "MsiGetProperty (CustomActionData) returned %d", er);
		return ERROR_INSTALL_FAILURE;
	}
	er = UninstallDriver (hInstall, CustomActionData);
	return er;
}

BOOL IsDriverInstalled (MSIHANDLE hInstall, char *DeviceId)
{
	HDEVINFO        hDevInfo;
	SP_DEVINFO_DATA DeviceInfoData;
	int j;
	BOOL bResult = FALSE;
	// scan all root-enumerated devices
	hDevInfo = SetupDiGetClassDevs (
		NULL,
		REGSTR_KEY_ROOTENUM,
		NULL,
		DIGCF_PRESENT|DIGCF_ALLCLASSES
		);
	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		DWORD dwErr = GetLastError();
		LogMessage (hInstall, "SetupDiGetClassDevs failed, dwErr = %d", dwErr);
		return FALSE;
	}

	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	for (j = 0; SetupDiEnumDeviceInfo (hDevInfo, j, &DeviceInfoData); j++)
	{
		LPTSTR buffer = NULL;
		DWORD buffersize = 0;

		//
		// Call function with null to begin with,
		// then use the returned buffer size
		// to Alloc the buffer. Keep calling until
		// success or an unknown failure.
		//
	    DEVPROPTYPE PropertyType;
		while (!SetupDiGetDeviceRegistryProperty(
			hDevInfo,
			&DeviceInfoData,
			SPDRP_HARDWAREID,
			&PropertyType,
			(PBYTE)buffer,
			buffersize,
			&buffersize))
		{
			if (GetLastError() ==
				ERROR_INSUFFICIENT_BUFFER)
			{
				// Change the buffer size.
				if (buffer) LocalFree(buffer);
				buffer = (LPTSTR)LocalAlloc(LPTR,buffersize);
			}
			else
			{
				break;
			}
		}

		if (buffer)
		{
			if (_strcmpi (buffer, DeviceId) == 0)
			{
				bResult = TRUE;
			}
			LocalFree(buffer);
		}
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);
	return bResult;
}

UINT __stdcall CheckInstall (MSIHANDLE hInstall)
{
	HRESULT hr = S_OK;
	UINT er = ERROR_SUCCESS;

	{
		wsprintf (buf, "CheckInstall entry\n");
		OutputDebugString (buf);
	}

	char DeviceId[MAX_PATH];
	DWORD n = sizeof DeviceId;
	er = MsiGetProperty (hInstall, "DEVICEIDFORCA", DeviceId, &n);
	if (er != ERROR_SUCCESS)
	{
		LogMessage (hInstall, "MsiGetProperty (DEVICEIDFORCA) returned %d\n", er);
		return ERROR_INSTALL_FAILURE;
	}
	BOOL b = IsDriverInstalled (hInstall, DeviceId);

	{
		wsprintf (buf, "CheckInstall exit, b = %d\n", b);
		OutputDebugString (buf);
	}

	if (b)
		MsiSetProperty (hInstall, "DRIVERINSTALLED", "1");
	return ERROR_SUCCESS;
}

extern "C" BOOL WINAPI DllMain(
	__in HINSTANCE hInst,
	__in ULONG ulReason,
	__in LPVOID
	)
{

	switch(ulReason)
	{
	case DLL_PROCESS_ATTACH:
		break;

	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}
