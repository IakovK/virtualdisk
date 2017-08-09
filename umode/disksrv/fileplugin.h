#ifndef __FILEPLUGIN_H__
#define __FILEPLUGIN_H__

#include "ifaces.h"

class CFilePlugin : public IPlugin
{
public:
	virtual BOOL Init (LPCWSTR cmdLine);
	virtual IDiskStorage *CreateDisk (LPCWSTR diskName, LPVOID context);
	virtual void Release();
};

class CFileStorage : public IDiskStorage
{
	HANDLE m_Handle;
	ULONG m_BlockSize;
	ULONG m_nBlocks;
	~CFileStorage();
public:
	CFileStorage();
    __drv_arg(this, __drv_freesMem(object))
	virtual void Release();
	virtual BOOL Init (LPCWSTR diskName);
	virtual BOOL ReadBlocks (ULONG logicalBlockAddress, ULONG transferBlocks, PVOID buffer, PULONG nBytes);
	virtual BOOL WriteBlocks (ULONG logicalBlockAddress, ULONG transferBlocks, PVOID buffer, PULONG nBytes);
	virtual ULONG GetNumBlocks();
	virtual ULONG GetBlockSize();
};

#endif
