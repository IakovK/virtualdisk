#ifndef __VIXWRAPPER_H__
#define __VIXWRAPPER_H__

#include "vixDiskLib.h"

typedef
VixError
(*PVIXDISKLIB_INIT)(uint32 majorVersion,
                uint32 minorVersion,
                VixDiskLibGenericLogFunc *log,
                VixDiskLibGenericLogFunc *warn,
                VixDiskLibGenericLogFunc *panic,
                const char* libDir);
typedef
void
(*PVIXDISKLIB_EXIT)(void);
typedef
VixError
(*PVIXDISKLIB_CONNECT)(const VixDiskLibConnectParams *connectParams,
                   VixDiskLibConnection *connection);
typedef
VixError
(*PVIXDISKLIB_DISCONNECT)(VixDiskLibConnection connection);
typedef
VixError
(*PVIXDISKLIB_OPEN)(const VixDiskLibConnection connection,
                const char *path,
                uint32 flags,
                VixDiskLibHandle *diskHandle);

typedef
VixError
(*PVIXDISKLIB_GETINFO)(VixDiskLibHandle diskHandle,
                   VixDiskLibInfo **info);

typedef
void
(*PVIXDISKLIB_FREEINFO)(VixDiskLibInfo *info);

typedef
VixError
(*PVIXDISKLIB_CLOSE)(VixDiskLibHandle diskHandle);

typedef
VixError
(*PVIXDISKLIB_READ)(VixDiskLibHandle diskHandle,
                VixDiskLibSectorType startSector,
                VixDiskLibSectorType numSectors,
                uint8 *readBuffer);

typedef
VixError
(*PVIXDISKLIB_WRITE)(VixDiskLibHandle diskHandle,
                 VixDiskLibSectorType startSector,
                 VixDiskLibSectorType numSectors,
                 const uint8 *writeBuffer);
//VixDiskLib_Disconnect
//VixDiskLib_Close
//VixDiskLib_FreeInfo
//VixDiskLib_GetInfo
//VixDiskLib_Open
//VixDiskLib_Read
//VixDiskLib_Write
class CVixWrapper
{
public:
	PVIXDISKLIB_INIT pVixDiskLib_InitFunc;
	PVIXDISKLIB_EXIT pVixDiskLib_ExitFunc;
	PVIXDISKLIB_CONNECT pVixDiskLib_ConnectFunc;
	PVIXDISKLIB_DISCONNECT pVixDiskLib_DisconnectFunc;
	PVIXDISKLIB_OPEN pVixDiskLib_OpenFunc;
	PVIXDISKLIB_GETINFO pVixDiskLib_GetInfoFunc;
	PVIXDISKLIB_FREEINFO pVixDiskLib_FreeInfoFunc;
	PVIXDISKLIB_CLOSE pVixDiskLib_CloseFunc;
	PVIXDISKLIB_READ pVixDiskLib_ReadFunc;
	PVIXDISKLIB_WRITE pVixDiskLib_WriteFunc;
	CVixWrapper():
	pVixDiskLib_InitFunc(NULL),
	pVixDiskLib_ExitFunc(NULL),
	pVixDiskLib_ConnectFunc(NULL),
	pVixDiskLib_DisconnectFunc(NULL),
	pVixDiskLib_OpenFunc(NULL),
	pVixDiskLib_GetInfoFunc(NULL),
	pVixDiskLib_FreeInfoFunc(NULL),
	pVixDiskLib_CloseFunc(NULL),
	pVixDiskLib_ReadFunc(NULL),
	pVixDiskLib_WriteFunc(NULL)
	{}
	BOOL Init (LPCWSTR VixLibPath);
};

#endif
