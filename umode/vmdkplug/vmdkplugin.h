#ifndef __VMDKPLUGIN_H__
#define __VMDKPLUGIN_H__

#include "ifaces.h"
#include "VixWrapper.h"

class CVmdkPlugin : public IPlugin
{
	VixDiskLibConnection m_pConnection;
	BOOL m_bInited;
public:
	CVmdkPlugin():m_bInited(FALSE){}
	virtual BOOL Init (LPCWSTR VixLibPath);
	virtual IDiskStorage *CreateDisk (LPCWSTR diskName, LPVOID context);
	virtual void Release();
};

class CVmdkStorage : public IDiskStorage
{
	VixDiskLibConnection m_pConnection;
	VixDiskLibHandle m_Handle;
	ULONG m_BlockSize;
	ULONG m_nBlocks;
	~CVmdkStorage();
public:
	CVmdkStorage(VixDiskLibConnection pConnection);
    __drv_arg(this, __drv_freesMem(object))
	virtual void Release();
	virtual BOOL Init (LPCWSTR diskName);
	virtual BOOL ReadBlocks (ULONG logicalBlockAddress, ULONG transferBlocks, PVOID buffer, PULONG nBytes);
	virtual BOOL WriteBlocks (ULONG logicalBlockAddress, ULONG transferBlocks, PVOID buffer, PULONG nBytes);
	virtual ULONG GetNumBlocks();
	virtual ULONG GetBlockSize();
};

#endif
