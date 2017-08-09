#ifndef __IFACES_H__
#define __IFACES_H__

class IDiskStorage
{
public:
	virtual void Release() = 0;
	virtual BOOL Init (LPCWSTR diskName) = 0;
	virtual BOOL ReadBlocks (ULONG logicalBlockAddress, ULONG transferBlocks, PVOID buffer, PULONG nBytes) = 0;
	virtual BOOL WriteBlocks (ULONG logicalBlockAddress, ULONG transferBlocks, PVOID buffer, PULONG nBytes) = 0;
	virtual ULONG GetNumBlocks() = 0;
	virtual ULONG GetBlockSize() = 0;
};

class IPlugin
{
public:
	virtual BOOL Init (LPCWSTR cmdLine) = 0;
	virtual IDiskStorage *CreateDisk (LPCWSTR diskName, LPVOID context) = 0;
	virtual void Release() = 0;
};

typedef IPlugin *PPlugin;

typedef PPlugin (*PluginFunc)();

#endif
