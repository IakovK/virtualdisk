#include <windows.h>
#include "fileplugin.h"

extern "C" PPlugin FilePluginFunc()
{
	try
	{
		PPlugin pp = new CFilePlugin;
		return pp;
	}
	catch (...)
	{
		return NULL;
	}
	return NULL;
}

BOOL CFilePlugin::Init (LPCWSTR cmdLine)
{
	return TRUE;
}

IDiskStorage *CFilePlugin::CreateDisk (LPCWSTR diskName, LPVOID context)
{
	try
	{
		CFileStorage *fs = new CFileStorage;
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

void CFilePlugin::Release()
{
	delete this;
}

CFileStorage::CFileStorage()
{
	m_Handle = NULL;
	m_BlockSize = 0;
	m_nBlocks = 0;
}

CFileStorage::~CFileStorage()
{
	if (m_Handle)
		CloseHandle (m_Handle);
}

void CFileStorage::Release()
{
	delete this;
}

BOOL CFileStorage::Init (LPCWSTR diskName)
{
	m_Handle = CreateFileW (diskName, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (m_Handle == INVALID_HANDLE_VALUE)
	{
		m_Handle = NULL;
		return FALSE;
	}
	LARGE_INTEGER fileSize;
	BOOL b = GetFileSizeEx (m_Handle, &fileSize);
	if (!b)
	{
		return FALSE;
	}
	if (fileSize.HighPart >= 512)	// file too large
	{
		return FALSE;
	}
	m_BlockSize = 512;
	m_nBlocks = (ULONG)(fileSize.QuadPart / 512);
	return TRUE;
}

BOOL CFileStorage::ReadBlocks (ULONG logicalBlockAddress, ULONG transferBlocks, PVOID buffer, PULONG nBytes)
{
	BOOL b;
	LARGE_INTEGER pos;
	ULONG length = transferBlocks * m_BlockSize;
	pos.QuadPart = (LONGLONG)logicalBlockAddress * m_BlockSize;
	b = SetFilePointerEx (m_Handle, pos, NULL, FILE_BEGIN);
	if (b)
		b = ReadFile (m_Handle, buffer, length, nBytes, NULL);
	return b;
}

BOOL CFileStorage::WriteBlocks (ULONG logicalBlockAddress, ULONG transferBlocks, PVOID buffer, PULONG nBytes)
{
	BOOL b;
	LARGE_INTEGER pos;
	ULONG length = transferBlocks * m_BlockSize;
	pos.QuadPart = (LONGLONG)logicalBlockAddress * m_BlockSize;
	b = SetFilePointerEx (m_Handle, pos, NULL, FILE_BEGIN);
	if (b)
	{
		b = WriteFile (m_Handle, buffer, length, nBytes, NULL);
	}
	return b;
}

ULONG CFileStorage::GetNumBlocks()
{
	return m_nBlocks;
}

ULONG CFileStorage::GetBlockSize()
{
	return m_BlockSize;
}
