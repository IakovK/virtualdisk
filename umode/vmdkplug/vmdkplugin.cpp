#include <windows.h>
#include <string>

#include "vmdkplugin.h"
#include <AtlBase.h>
#include "VixWrapper.h"

#define VIXDISKLIB_VERSION_MAJOR 1
#define VIXDISKLIB_VERSION_MINOR 0

CVixWrapper g_VixWrapper;

extern "C" PPlugin OpenPlugin()
{
	try
	{
		PPlugin pp = new CVmdkPlugin;
		return pp;
	}
	catch (...)
	{
		return NULL;
	}
	return NULL;
}

void logFunc (const char *fmt, va_list args)
{
	static char buf[1000];
	vsprintf_s (buf, 1000, fmt, args);
	OutputDebugString (buf);
	OutputDebugString ("\n");
}

BOOL CVmdkPlugin::Init (LPCWSTR VixLibPath)
{
	BOOL b = g_VixWrapper.Init (VixLibPath);
	if (!b)
		return b;

	VixError vixError = g_VixWrapper.pVixDiskLib_InitFunc(VIXDISKLIB_VERSION_MAJOR,
		VIXDISKLIB_VERSION_MINOR,
		logFunc, logFunc, logFunc, // Log, warn, panic
		CW2A(VixLibPath));   // libDir
	if (vixError != VIX_OK)
	{
		m_bInited = FALSE;
		return FALSE;
	}
	VixDiskLibConnectParams cnxParams = {0};
	vixError = g_VixWrapper.pVixDiskLib_ConnectFunc(&cnxParams,
		&m_pConnection);
	if (vixError != VIX_OK)
	{
		g_VixWrapper.pVixDiskLib_ExitFunc();
		m_bInited = FALSE;
		return FALSE;
	}
	m_bInited = TRUE;
	return TRUE;
}

IDiskStorage *CVmdkPlugin::CreateDisk (LPCWSTR diskName, LPVOID context)
{
	try
	{
		CVmdkStorage *fs = new CVmdkStorage (m_pConnection);
		if (!fs->Init (diskName))
		{
			fs->Release();
			return NULL;
		}
		return fs;
	}
	catch (...)
	{
		return NULL;
	}
	return NULL;
}

void CVmdkPlugin::Release()
{
	if (m_bInited)
	{
		g_VixWrapper.pVixDiskLib_DisconnectFunc(m_pConnection);
		g_VixWrapper.pVixDiskLib_ExitFunc();
	}
	delete this;
}

CVmdkStorage::CVmdkStorage(VixDiskLibConnection pConnection)
{
	m_pConnection = pConnection;
	m_Handle = NULL;
	m_BlockSize = 0;
	m_nBlocks = 0;
}

CVmdkStorage::~CVmdkStorage()
{
	if (m_Handle)
		g_VixWrapper.pVixDiskLib_CloseFunc(m_Handle);
}

void CVmdkStorage::Release()
{
	delete this;
}

BOOL CVmdkStorage::Init (LPCWSTR diskName)
{
	// convert name from UNICODE to ANSI
	CW2A pc (diskName);
	VixError vixError = g_VixWrapper.pVixDiskLib_OpenFunc (m_pConnection, pc,
		0,
		&m_Handle);
	if (vixError != VIX_OK)
	{
		return FALSE;
	}
	VixDiskLibInfo *diskInfo = NULL;
	vixError = g_VixWrapper.pVixDiskLib_GetInfoFunc(m_Handle, &diskInfo);
	if (vixError != VIX_OK)
	{
		return FALSE;
	}
	m_BlockSize = VIXDISKLIB_SECTOR_SIZE;
	if (diskInfo->capacity > MAX_UINT32)	// file too large
	{
		g_VixWrapper.pVixDiskLib_FreeInfoFunc(diskInfo);
		return FALSE;
	}
	m_nBlocks = (ULONG)(diskInfo->capacity);
	g_VixWrapper.pVixDiskLib_FreeInfoFunc(diskInfo);
	return TRUE;
}

BOOL CVmdkStorage::ReadBlocks (ULONG logicalBlockAddress, ULONG transferBlocks, PVOID buffer, PULONG nBytes)
{
	VixError vixError = g_VixWrapper.pVixDiskLib_ReadFunc(m_Handle,
		logicalBlockAddress,
		transferBlocks,
		(uint8 *)buffer);

	if (vixError != VIX_OK)
	{
		*nBytes = 0;
		return FALSE;
	}
	else
	{
		*nBytes = transferBlocks * m_BlockSize;
		return TRUE;
	}
}

BOOL CVmdkStorage::WriteBlocks (ULONG logicalBlockAddress, ULONG transferBlocks, PVOID buffer, PULONG nBytes)
{
	VixError vixError = g_VixWrapper.pVixDiskLib_WriteFunc(m_Handle,
		logicalBlockAddress,
		transferBlocks,
		(uint8 *)buffer);

	if (vixError != VIX_OK)
	{
		*nBytes = 0;
		return FALSE;
	}
	else
	{
		*nBytes = transferBlocks * m_BlockSize;
		return TRUE;
	}
}

ULONG CVmdkStorage::GetNumBlocks()
{
	return m_nBlocks;
}

ULONG CVmdkStorage::GetBlockSize()
{
	return m_BlockSize;
}
