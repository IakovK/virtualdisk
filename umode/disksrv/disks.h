#ifndef __DISKS_H__
#define __DISKS_H__

#include "ifaces.h"
#include "public.h"
#include <map>

using namespace std; 

#define LOG_NAME "disksrv.log"

#define SERVICE_NAME "disksrv"
#define SERVICE_DISPLAY_NAME "virtual disks service"
#define EP_NAME "disksrv"

class CDiskServer;

class CPluginModule
{
	string m_dllName;	// argument for LoadLibrary
	HMODULE m_hModule;	// result of LoadLibrary
	PluginFunc m_func;
public:
	CPluginModule (LPCSTR fileName);
	PluginFunc Load();
};

class CPluginManager
{
	typedef pair <wstring, IPlugin *> PluginPair;
	typedef map <wstring, IPlugin *> PluginMap;
	PluginMap m_pluginMap;
	CComCriticalSection m_cs;
public:
	CPluginManager();
	~CPluginManager();
	IPlugin *FindPlugin (LPCWSTR diskType);
	bool AddPlugin (LPCWSTR diskType, PluginFunc func, LPCWSTR cmdLine);
	bool AddExternalPlugin (HKEY key, LPCSTR diskType);
	bool AddExternalPlugins ();
	void DeleteAllPlugins();
};

class CDiskProcessor
{
	PVOID m_Buffer;
	HANDLE m_hEvent;
	HANDLE m_hRemoveEvent;
	HANDLE m_hThread;
	ULONG m_diskId;
	IDiskStorage *m_pStorage;
	CDiskServer *m_pServer;
	BUSENUM_REQUEST m_req;

	static UINT __stdcall threadProcThunk (PVOID context);
	UINT threadProc ();
public:
	CDiskProcessor(CDiskServer *ds, ULONG diskId);
	~CDiskProcessor();
	void ProcessRequest (const BUSENUM_REQUEST &req);
	BOOL Init (LPCWSTR diskType, LPCWSTR diskName);
	PVOID GetBuffer ();
	ULONG GetNumBlocks();
	ULONG GetBlockSize();
	HANDLE GetRemoveEvent();
};

class CDiskContainer
{
	typedef pair <int, CDiskProcessor *> DiskPair;
	typedef map<ULONG, CDiskProcessor *> DiskMap;
	DiskMap m_diskMap;
	CComCriticalSection m_cs;
public:
	CDiskContainer();
	CDiskProcessor *FindDisk (ULONG handle);
	bool AddDisk (ULONG diskId, CDiskProcessor *disk);
	void RemoveDisk (ULONG diskId);
};

class CDiskServer
{
	enum {RequestEvent = 0, TerminationEvent};
	HANDLE hFile;	// handle to device interface
	HANDLE hEvents[2];	// handles to event signaled when request is ready and to event set in ConsoleCtrlHandler
						// (arranged for WaitForMultipleObjects);
	CDiskContainer dc;
	CPluginManager plMan;

	// SCM related stuff
	SERVICE_STATUS Status;
	SERVICE_STATUS_HANDLE StatusHandle;
	static VOID WINAPI CtrlHandler (DWORD Opcode);

	BOOL GetRequest (BUSENUM_REQUEST &req);
	BOOL RegisterWithDriver (ULONG diskId, CDiskProcessor *disk);
	BOOL UnregisterWithDriver (ULONG diskId);
	void RegisterPlugins();
	void UnregisterPlugins();
	CDiskProcessor *CreateDiskProcessor (ULONG diskId, LPCWSTR diskType, LPCWSTR diskName);
public:
	static VOID WINAPI ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv);
	BOOL SendReply (ULONG handle, ULONG code, ULONG nBytes, BOOL status);
	BOOL AddDisk (ULONG diskId, LPCWSTR diskType, LPCWSTR diskName);
	void RemoveDisk (ULONG diskId);
	void RemoveDiskFinal (ULONG diskId);
	BOOL InitRpc();
	BOOL Init();
	void Run();
	void Terminate();
	IDiskStorage *CreateDiskStorage (LPCWSTR diskType, LPCWSTR diskName);
};

#endif
