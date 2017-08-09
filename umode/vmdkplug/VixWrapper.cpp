#include <atlbase.h>
#include <string>
#include <stdlib.h>

#include "VixWrapper.h"

BOOL CVixWrapper::Init (LPCWSTR VixLibPath)
{
	std::string path = CW2A (VixLibPath);
	char dllPath[MAX_PATH];
	_makepath (dllPath, NULL, path.c_str(), "vixDiskLib.dll", NULL);
	HMODULE hm = LoadLibrary (dllPath);
	if (hm == NULL)
		return FALSE;
	pVixDiskLib_InitFunc = (PVIXDISKLIB_INIT)GetProcAddress (hm, "VixDiskLib_Init");
	if (pVixDiskLib_InitFunc == NULL)
		return FALSE;
	pVixDiskLib_ExitFunc = (PVIXDISKLIB_EXIT)GetProcAddress (hm, "VixDiskLib_Exit");
	if (pVixDiskLib_ExitFunc == NULL)
		return FALSE;
	pVixDiskLib_ConnectFunc = (PVIXDISKLIB_CONNECT)GetProcAddress (hm, "VixDiskLib_Connect");
	if (pVixDiskLib_ConnectFunc == NULL)
		return FALSE;
	pVixDiskLib_DisconnectFunc = (PVIXDISKLIB_DISCONNECT)GetProcAddress (hm, "VixDiskLib_Disconnect");
	if (pVixDiskLib_DisconnectFunc == NULL)
		return FALSE;
	pVixDiskLib_OpenFunc = (PVIXDISKLIB_OPEN)GetProcAddress (hm, "VixDiskLib_Open");
	if (pVixDiskLib_ConnectFunc == NULL)
		return FALSE;
	pVixDiskLib_GetInfoFunc = (PVIXDISKLIB_GETINFO)GetProcAddress (hm, "VixDiskLib_GetInfo");
	if (pVixDiskLib_ConnectFunc == NULL)
		return FALSE;
	pVixDiskLib_FreeInfoFunc = (PVIXDISKLIB_FREEINFO)GetProcAddress (hm, "VixDiskLib_FreeInfo");
	if (pVixDiskLib_ConnectFunc == NULL)
		return FALSE;
	pVixDiskLib_CloseFunc = (PVIXDISKLIB_CLOSE)GetProcAddress (hm, "VixDiskLib_Close");
	if (pVixDiskLib_ConnectFunc == NULL)
		return FALSE;
	pVixDiskLib_ReadFunc = (PVIXDISKLIB_READ)GetProcAddress (hm, "VixDiskLib_Read");
	if (pVixDiskLib_ReadFunc == NULL)
		return FALSE;
	pVixDiskLib_WriteFunc = (PVIXDISKLIB_WRITE)GetProcAddress (hm, "VixDiskLib_Write");
	if (pVixDiskLib_WriteFunc == NULL)
		return FALSE;
	return TRUE;
}

