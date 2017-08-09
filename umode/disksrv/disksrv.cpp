#include <windows.h>
#include <initguid.h>
#include <winioctl.h>
#include <setupapi.h>
#include <scsi.h>
#include <string>
#include <coguid.h>
#include <atlbase.h>
#include <algorithm>
#include <locale>

typedef LONG NTSTATUS;
#define STATUS_SUCCESS                          ((NTSTATUS)0x00000000L) // ntsubauth
#define STATUS_UNSUCCESSFUL              ((NTSTATUS)0xC0000001L)

#include "disksrv.h"
#include "disks.h"
#include "filelog.h"

ILog *g_pLog;

HANDLE OpenBusInterface (const GUID &guid)
{
    HDEVINFO                    hardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA    deviceInterfaceData;
    //
    // Open a handle to the device interface information set of all
    // present toaster bus enumerator interfaces.
    //

    hardwareDeviceInfo = SetupDiGetClassDevs (
                       (LPGUID)&guid,
                       NULL, // Define no enumerator (global)
                       NULL, // Define no
                       (DIGCF_PRESENT | // Only Devices present
                       DIGCF_DEVICEINTERFACE)); // Function class devices.

    if(INVALID_HANDLE_VALUE == hardwareDeviceInfo)
    {
		return INVALID_HANDLE_VALUE;
    }
    deviceInterfaceData.cbSize = sizeof (SP_DEVICE_INTERFACE_DATA);

    if (!SetupDiEnumDeviceInterfaces (hardwareDeviceInfo,
                                 0, // No care about specific PDOs
                                 (LPGUID)&guid,
                                 0, //
                                 &deviceInterfaceData))
	{
		return INVALID_HANDLE_VALUE;
    }

	//
    // Allocate a function class device data structure to receive the
    // information about this particular device.
    //

    ULONG                               predictedLength = 0;
    ULONG                               requiredLength = 0;
    PSP_DEVICE_INTERFACE_DETAIL_DATA    deviceInterfaceDetailData = NULL;
    SetupDiGetDeviceInterfaceDetail (
            hardwareDeviceInfo,
            &deviceInterfaceData,
            NULL, // probing so no output buffer yet
            0, // probing so output buffer length of zero
            &requiredLength,
            NULL); // not interested in the specific dev-node

    if(ERROR_INSUFFICIENT_BUFFER != GetLastError())
	{
		return INVALID_HANDLE_VALUE;
    }

	predictedLength = requiredLength;
    deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)_alloca (predictedLength);
    deviceInterfaceDetailData->cbSize =
                  sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA);

    if (! SetupDiGetDeviceInterfaceDetail (
               hardwareDeviceInfo,
               &deviceInterfaceData,
               deviceInterfaceDetailData,
               predictedLength,
               &requiredLength,
               NULL))
	{
		return INVALID_HANDLE_VALUE;
    }

    HANDLE file = CreateFile ( deviceInterfaceDetailData->DevicePath,
                        GENERIC_READ, // Only read access
                        0,
                        NULL, // no SECURITY_ATTRIBUTES structure
                        OPEN_EXISTING, // No special create flags
                        FILE_FLAG_OVERLAPPED,
                        NULL); // No template file

    SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
	return file;
}

CPluginModule::CPluginModule (LPCSTR fileName)
{
	m_dllName = fileName;
	m_hModule = NULL;
	m_func = NULL;
}

PluginFunc CPluginModule::Load()
{
	m_hModule = LoadLibrary (m_dllName.c_str());
	if (m_hModule == NULL)
		return NULL;
	m_func = (PluginFunc)GetProcAddress (m_hModule, "OpenPlugin");
	return m_func;
}

CPluginManager::CPluginManager()
{
	m_cs.Init();
}

CPluginManager::~CPluginManager()
{
	DeleteAllPlugins();
}

IPlugin *CPluginManager::FindPlugin (LPCWSTR diskType)
{
	std::wstring ws(diskType);
	std::transform (ws.begin(), ws.end(), ws.begin(), towlower);
	PluginPair pp;
	PluginMap::iterator iter = m_pluginMap.find (ws);
	if (iter == m_pluginMap.end())
		return NULL;
	else
		return iter->second;
}

bool CPluginManager::AddPlugin (LPCWSTR diskType, PluginFunc func, LPCWSTR cmdLine)
{
	try
	{
	PPlugin Plugin = func();
	if (Plugin == NULL)
		return false;
	if (!Plugin->Init (cmdLine))
	{
		Plugin->Release();
		return false;
	}
	std::wstring ws(diskType);
	std::transform (ws.begin(), ws.end(), ws.begin(), towlower);
	CComCritSecLock <CComCriticalSection> lock (m_cs);
	PluginPair pp (ws, Plugin);
	std::pair <PluginMap::iterator, bool> pr;
	pr = m_pluginMap.insert (pp);
	return pr.second;
	}
	catch(...)
	{
		return false;
	}
}

bool CPluginManager::AddExternalPlugin (HKEY key, LPCSTR diskType)
{
	if (FindPlugin (CA2W(diskType)))
		return true;
	CRegKey rk;
	if (ERROR_SUCCESS != rk.Open (key, diskType))
		return false;
	char cmdLine[MAX_PATH];
	char fileName[MAX_PATH];
	DWORD lineLen = sizeof cmdLine;
	DWORD nameLen = sizeof fileName;
	if (ERROR_SUCCESS != rk.QueryStringValue ("fileName", fileName, &nameLen))
		return false;
	CPluginModule pm (fileName);
	PluginFunc func = pm.Load();
	bool b;
	if (ERROR_SUCCESS == rk.QueryStringValue ("options", cmdLine, &lineLen))
		b = AddPlugin (CA2W(diskType), func, CA2W(cmdLine));
	else
		b = AddPlugin (CA2W(diskType), func, NULL);
	return b;
}

bool CPluginManager::AddExternalPlugins ()
{
	CRegKey rk;
	if (ERROR_SUCCESS != rk.Open (HKEY_LOCAL_MACHINE, (std::string("SYSTEM\\CurrentControlSet\\Services\\")
		+ std::string(SERVICE_NAME) + std::string("\\Plugins")).c_str()))
		return false;
	for (ULONG j = 0;; j++)
	{
		char type[MAX_PATH];
		DWORD typeLen = sizeof type;
		LONG err = rk.EnumKey (j, type, &typeLen);
		if (err != ERROR_SUCCESS)
			break;
		AddExternalPlugin (rk, type);
	}
	return true;
}

void CPluginManager::DeleteAllPlugins()
{
	PluginMap::iterator iter;
	for (iter = m_pluginMap.begin(); iter != m_pluginMap.end(); iter++)
	{
		iter->second->Release();
	}
	m_pluginMap.erase (m_pluginMap.begin(), m_pluginMap.end());
}

CDiskProcessor::CDiskProcessor(CDiskServer *ds, ULONG diskId)
{
	m_Buffer = NULL;
	m_hEvent = NULL;
	m_hRemoveEvent = NULL;
	m_diskId = diskId;
	m_pStorage = NULL;
	m_hThread = NULL;
	m_pServer = ds;
}

CDiskProcessor::~CDiskProcessor()
{
	g_pLog->Format ("CDiskProcessor::~CDiskProcessor:"
		" m_diskId = %d, m_hEvent = %08x, m_hRemoveEvent = %08x\n", m_diskId, m_hEvent, m_hRemoveEvent);
	if (m_Buffer)
		VirtualFree (m_Buffer, 0, MEM_RELEASE);
	if (m_hEvent)
		CloseHandle (m_hEvent);
	if (m_hRemoveEvent)
		CloseHandle (m_hRemoveEvent);
	try
	{
	if (m_pStorage)
		m_pStorage->Release();
	}
	catch(...)
	{
	}
	if (m_hThread)
		CloseHandle (m_hThread);
}

BOOL CDiskProcessor::Init (LPCWSTR diskType, LPCWSTR diskName)
{
	// all the allocated resources will be freed in destructor
	m_Buffer = VirtualAlloc (NULL, BUFFER_SIZE, MEM_COMMIT, PAGE_READWRITE);
	if (m_Buffer == NULL)
		return FALSE;
	// create auto reset event. Only one thread (created below) will be waiting for it.
	m_hEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (m_hEvent == NULL)
		return FALSE;
	// This event will be passed to driver and signaled when driver removes PDO
	m_hRemoveEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (m_hRemoveEvent == NULL)
		return FALSE;
	m_pStorage = m_pServer->CreateDiskStorage (diskType, diskName);
	if (m_pStorage == NULL)
		return FALSE;
	m_hThread = (HANDLE)_beginthreadex (NULL, 0, threadProcThunk, this, 0, NULL);
	if (m_hThread == NULL)
		return FALSE;
	return TRUE;
}

UINT CDiskProcessor::threadProc ()
{
	BOOL b = FALSE;
	ULONG nBytes = 0;
	HANDLE events[2] = {m_hEvent, m_hRemoveEvent};
	g_pLog->Format ("CDiskProcessor::threadProc:"
		" starting, m_diskId = %d, m_hEvent = %08x, m_hRemoveEvent = %08x\n", m_diskId, m_hEvent, m_hRemoveEvent);
	while (TRUE)
	{
//		g_pLog->Format ("CDiskProcessor::threadProc: waiting for events...\n");
		DWORD s = WaitForMultipleObjects (2, events, FALSE, INFINITE);
//		g_pLog->Format ("CDiskProcessor::threadProc: WaitForMultipleObjects returned: %d\n", s);
		if (s == WAIT_OBJECT_0 + 1)	// PDO was removed
		{
			g_pLog->Format ("CDiskProcessor::threadProc: calling RemoveDiskFinal (m_diskId)\n");
			m_pServer->RemoveDiskFinal (m_diskId);
			g_pLog->Format ("CDiskProcessor::threadProc: calling RemoveDiskFinal (m_diskId) done\n");
			break;
		}
		if (s == WAIT_FAILED)
		{
			DWORD dwErr = GetLastError();
			g_pLog->Format ("wait failed: dwErr = %d\n", dwErr);
			break;
		}
		switch (m_req.code)
		{
			DWORD dwErr;
		case SCSIOP_READ:
			try
			{
				b = m_pStorage->ReadBlocks (m_req.logicalBlockAddress, m_req.transferBlocks, m_Buffer, &nBytes);
			}
			catch (...)
			{
				b = FALSE;
			}
			if (!b)
			{
				dwErr = GetLastError();
				g_pLog->Format ("CDiskProcessor::threadProc:"
					" m_pStorage->ReadBlocks(m_diskId = %d, logicalBlockAddress = %d, transferBlocks = %d)"
					" failed: dwErr = %d\n", m_diskId, m_req.logicalBlockAddress, m_req.transferBlocks, dwErr);
			}
			break;
		case SCSIOP_WRITE:
			try
			{
				b = m_pStorage->WriteBlocks (m_req.logicalBlockAddress, m_req.transferBlocks, m_Buffer, &nBytes);
			}
			catch (...)
			{
				b = FALSE;
			}
			if (!b)
			{
				dwErr = GetLastError();
				g_pLog->Format ("CDiskProcessor::threadProc:"
					" m_pStorage->WriteBlocks(m_diskId = %d, logicalBlockAddress = %d, transferBlocks = %d)"
					" failed: dwErr = %d\n", m_diskId, m_req.logicalBlockAddress, m_req.transferBlocks, dwErr);
			}
			break;
		}
		m_pServer->SendReply (m_req.handle, m_req.code, nBytes, b);
	}
	g_pLog->Format ("CDiskProcessor::threadProc: exiting\n");
	return 0;
}

UINT __stdcall CDiskProcessor::threadProcThunk (PVOID context)
{
	CDiskProcessor *ptr = (CDiskProcessor *)context;
	if (!context)
		return 0;
	return ptr->threadProc();
}

void CDiskProcessor::ProcessRequest (const BUSENUM_REQUEST &req)
{
	m_req = req;
	SetEvent (m_hEvent);
}

PVOID CDiskProcessor::GetBuffer()
{
	return m_Buffer;
}

HANDLE CDiskProcessor::GetRemoveEvent()
{
	return m_hRemoveEvent;
}

ULONG CDiskProcessor::GetNumBlocks()
{
	try
	{
		return m_pStorage->GetNumBlocks();
	}
	catch (...)
	{
		return 0;
	}
}

ULONG CDiskProcessor::GetBlockSize()
{
	try
	{
		return m_pStorage->GetBlockSize();
	}
	catch (...)
	{
		return 0;
	}
}

CDiskContainer::CDiskContainer()
{
	m_cs.Init();
}

bool CDiskContainer::AddDisk (ULONG diskId, CDiskProcessor *disk)
{
	CComCritSecLock <CComCriticalSection> lock (m_cs);
	DiskPair dp (diskId, disk);
	std::pair <DiskMap::iterator, bool> pr;
	pr = m_diskMap.insert (dp);
	return pr.second;
}

void CDiskContainer::RemoveDisk (ULONG diskId)
{
	CComCritSecLock <CComCriticalSection> lock (m_cs);
	CDiskProcessor *dp = FindDisk (diskId);
	if (dp)
		delete dp;
	m_diskMap.erase (diskId);
}

CDiskProcessor *CDiskContainer::FindDisk (ULONG handle)
{
	DiskPair dp;
	DiskMap::iterator iter = m_diskMap.find (handle);
	if (iter == m_diskMap.end())
		return NULL;
	else
		return iter->second;
}

BOOL CDiskServer::InitRpc()
{
	RPC_STATUS status;
	unsigned char * pszProtocolSequence = (unsigned char *)"ncalrpc";
	unsigned char * pszEndpoint    = (unsigned char *)EP_NAME;

	g_pLog->Format ("CDiskServer::InitRpc: enter...\n");
	status = RpcServerUseProtseqEp (pszProtocolSequence, RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
		pszEndpoint, NULL);
	g_pLog->Format ("CDiskServer::InitRpc: RpcServerUseProtseqEp returned: %d\n", status);
	if (status)
	{
		SetLastError (status);
		return FALSE;
	}

	status = RpcServerRegisterIf (disksrv_v1_0_s_ifspec, NULL, NULL);
	g_pLog->Format ("CDiskServer::InitRpc: RpcServerRegisterIf returned: %d\n", status);
	if (status)
	{
		SetLastError (status);
		return FALSE;
	}

	status = RpcServerListen (1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, TRUE);
	g_pLog->Format ("CDiskServer::InitRpc: RpcServerListen returned: %d\n", status);
	if (status) 
	{
		SetLastError (status);
		return FALSE;
	}
	g_pLog->Format ("CDiskServer::InitRpc: exit...\n");
	return TRUE;
}

extern "C" PPlugin FilePluginFunc();
//extern "C" PPlugin VmdkPluginFunc();
void CDiskServer::RegisterPlugins()
{
	plMan.AddPlugin (L"plain", FilePluginFunc, NULL);
	plMan.AddExternalPlugins ();
//	plMan.AddPlugin (L"vmware", VmdkPluginFunc);
}

void CDiskServer::UnregisterPlugins()
{
	plMan.DeleteAllPlugins();
}

BOOL CDiskServer::Init()
{
	BOOL b;
	DWORD n;

	// open our device interface
	hFile = OpenBusInterface (GUID_DEVINTERFACE_BUSENUM_VDISK);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	// open event handles
	hEvents[RequestEvent] = CreateEvent (NULL, FALSE, FALSE, NULL);
	hEvents[TerminationEvent] = CreateEvent (NULL, FALSE, FALSE, NULL);
	RegisterPlugins();
	b = InitRpc();
	if (!b)
	{
		DWORD dwErr = GetLastError();
		g_pLog->Format ("ds.InitRpc returned: dwErr = %d\n", dwErr);
		return FALSE;
	}
	return TRUE;
}

IDiskStorage *CDiskServer::CreateDiskStorage (LPCWSTR diskType, LPCWSTR diskName)
{
	try
	{
	IPlugin *plugin = plMan.FindPlugin (diskType);
	if (plugin == NULL)
	{
		SetLastError (ERROR_FILE_NOT_FOUND);
		return NULL;
	}
	IDiskStorage *ds = plugin->CreateDisk (diskName, NULL);
	return ds;
	}
	catch (...)
	{
		SetLastError (ERROR_UNKNOWN_EXCEPTION);
		return NULL;
	}
}

CDiskProcessor *CDiskServer::CreateDiskProcessor (ULONG diskId, LPCWSTR diskType, LPCWSTR diskName)
{
	try
	{
		CDiskProcessor *disk = new CDiskProcessor (this, diskId);
		if (!disk->Init (diskType, diskName))
		{
			delete disk;
			return NULL;
		}
		return disk;
	}
	catch (...)
	{
		return NULL;
	}
	return NULL;
}

BOOL CDiskServer::RegisterWithDriver (ULONG diskId, CDiskProcessor *disk)
{
	BUSENUM_ADD_DISK dd;
	dd.handle = diskId;
	dd.buffer = disk->GetBuffer();
	dd.nBlocks = disk->GetNumBlocks();
	dd.BlockSize = disk->GetBlockSize();
	dd.RemoveEvent = disk->GetRemoveEvent();

	ULONG n;
	BOOL b = DeviceIoControl (hFile, IOCTL_BUSENUM_ADD_DISK, &dd, sizeof dd, NULL, 0, &n, NULL);
	if (!b)
	{
		DWORD dwErr = GetLastError();
		g_pLog->Format ("CDiskServer::RegisterWithDriver: DeviceIoControl returned: dwErr = %d\n", dwErr);
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

BOOL CDiskServer::UnregisterWithDriver (ULONG diskId)
{
	BUSENUM_REMOVE_DISK dd;
	dd.handle = diskId;

	ULONG n;
	BOOL b = DeviceIoControl (hFile, IOCTL_BUSENUM_REMOVE_DISK, &dd, sizeof dd, NULL, 0, &n, NULL);
	if (!b)
	{
		DWORD dwErr = GetLastError();
		g_pLog->Format ("CDiskServer::UnregisterWithDriver: DeviceIoControl returned: dwErr = %d\n", dwErr);
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

BOOL CDiskServer::AddDisk (ULONG diskId, LPCWSTR diskType, LPCWSTR diskName)
{
	CDiskProcessor *disk = CreateDiskProcessor (diskId, diskType, diskName);
	if (disk == NULL)
		return FALSE;
	try
	{
		if (!dc.AddDisk (diskId, disk))
			throw diskId;
	}
	catch (...)
	{
		delete disk;
		return FALSE;
	}
	if (!RegisterWithDriver (diskId, disk))
	{
		dc.RemoveDisk (diskId);
		return FALSE;
	}
	return TRUE;
}

void CDiskServer::RemoveDisk (ULONG diskId)
{
	g_pLog->Format ("CDiskServer::RemoveDisk: diskId = %d\n", diskId);
	UnregisterWithDriver (diskId);
	g_pLog->Format ("CDiskServer::RemoveDisk done\n");
}

void CDiskServer::RemoveDiskFinal (ULONG diskId)
{
	g_pLog->Format ("CDiskServer::RemoveDiskFinal: diskId = %d\n", diskId);
	dc.RemoveDisk (diskId);
	g_pLog->Format ("CDiskServer::RemoveDiskFinal done\n");
}

BOOL CDiskServer::GetRequest (BUSENUM_REQUEST &req)
{
	OVERLAPPED o;
	DWORD dwErr;
	DWORD n;
	o.hEvent = hEvents[RequestEvent];
	BOOL b = DeviceIoControl (hFile, IOCTL_BUSENUM_GET_REQUEST, NULL, 0, &req, sizeof req, &n, &o);
	if (!b)
	{
		dwErr = GetLastError();
		if (dwErr != ERROR_IO_PENDING)
		{
			return FALSE;
		}
	}
	DWORD code = WaitForMultipleObjects (2, hEvents, FALSE, INFINITE);
	if (code == WAIT_OBJECT_0 + TerminationEvent)
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

BOOL CDiskServer::SendReply (ULONG handle, ULONG code, ULONG nBytes, BOOL status)
{
	BUSENUM_REPLY rep;
	BOOL b;
	ULONG n;
	rep.handle = handle;
	rep.status = status?STATUS_SUCCESS:STATUS_UNSUCCESSFUL;
	rep.code = code;
	rep.numBytes = nBytes;
	b = DeviceIoControl (hFile, IOCTL_BUSENUM_SEND_REPLY, &rep, sizeof rep, NULL, 0, &n, NULL);
	return b;
}

void CDiskServer::Run()
{
	BUSENUM_REQUEST req;
	while (GetRequest (req))
	{
		CDiskProcessor *disk = dc.FindDisk (req.handle);
		if (disk)
		{
			disk->ProcessRequest (req);
		}
	}
}

void CDiskServer::Terminate()
{
	RPC_STATUS s = RpcMgmtStopServerListening (NULL);
	g_pLog->Format ("CDiskServer::Terminate: RpcMgmtStopServerListening returned: %d\n", s);
	UnregisterPlugins();
	CloseHandle (hEvents[TerminationEvent]);
	CloseHandle (hEvents[RequestEvent]);
	CloseHandle (hFile);
}

CDiskServer srv;
void InitLog (DWORD logMask)
{
	g_pLog = new FileLog ("disksrv: ", LOG_NAME, logMask);
}

void CloseLog()
{
	g_pLog->Release();
	g_pLog = 0;
}

VOID WINAPI CDiskServer::CtrlHandler (DWORD Opcode)
{
	DWORD status;

	switch(Opcode) 
	{ 
	case SERVICE_CONTROL_CONTINUE: 
		srv.Status.dwCurrentState = SERVICE_RUNNING; 
		break; 

	case SERVICE_CONTROL_STOP: 
		g_pLog->Format ("[diskSrv] SERVICE_CONTROL_STOP received. stopping service\n"); 
		SetEvent (srv.hEvents[TerminationEvent]);
		srv.Status.dwWin32ExitCode = 0; 
		srv.Status.dwCurrentState  = SERVICE_STOP_PENDING; 
		srv.Status.dwCheckPoint    = 0; 
		srv.Status.dwWaitHint      = 0; 

		if (!SetServiceStatus (srv.StatusHandle, 
			&srv.Status))
		{ 
			status = GetLastError(); 
			g_pLog->Format ("[diskSrv] SetServiceStatus error %ld\n", status); 
		} 

		g_pLog->Format ("[diskSrv] Leaving diskSrvCtrlHandler\n",0); 
		return; 

	case SERVICE_CONTROL_INTERROGATE: 
		// Fall through to send current status. 
		break; 

	default: 
		g_pLog->Format (" [diskSrv] Unrecognized opcode %ld\n", Opcode); 
	} 

	// Send current status. 
	if (!SetServiceStatus (srv.StatusHandle,  &srv.Status)) 
	{ 
		status = GetLastError(); 
		g_pLog->Format (" [diskSrv] SetServiceStatus error %ld\n", status); 
	} 
	return; 
}

VOID WINAPI CDiskServer::ServiceMain(
							   DWORD dwArgc,
							   LPTSTR* lpszArgv
							   )
{
	g_pLog->Format ("CDiskServer::ServiceMain: entry: dwArgc = %d\n", dwArgc);
	DWORD status; 
	DWORD specificError; 

	srv.Status.dwServiceType        = SERVICE_WIN32; 
	srv.Status.dwCurrentState       = SERVICE_START_PENDING; 
	srv.Status.dwControlsAccepted   = SERVICE_ACCEPT_STOP; 
	srv.Status.dwWin32ExitCode      = 0; 
	srv.Status.dwServiceSpecificExitCode = 0; 
	srv.Status.dwCheckPoint         = 0; 
	srv.Status.dwWaitHint           = 0; 

	srv.StatusHandle = RegisterServiceCtrlHandler (SERVICE_NAME, CDiskServer::CtrlHandler); 

	if (srv.StatusHandle == (SERVICE_STATUS_HANDLE)0) 
	{ 
		g_pLog->Format ("[disksrv] RegisterServiceCtrlHandler failed %d\n", GetLastError()); 
		return; 
	} 

	// Initialization code goes here. 
	BOOL b = srv.Init();
	if (!b)
	{
		DWORD dwErr = GetLastError();
		srv.Status.dwCurrentState       = SERVICE_STOPPED;
		srv.Status.dwCheckPoint         = 0;
		srv.Status.dwWaitHint           = 0;
		srv.Status.dwWin32ExitCode      = dwErr;
		srv.Status.dwServiceSpecificExitCode = 0;

		SetServiceStatus (srv.StatusHandle, &srv.Status);
		return;
	}

	// Initialization complete - report running status. 
	srv.Status.dwCurrentState       = SERVICE_RUNNING;
	srv.Status.dwCheckPoint         = 0;
	srv.Status.dwWaitHint           = 0;

	if (!SetServiceStatus (srv.StatusHandle, &srv.Status))
	{
		status = GetLastError();
		g_pLog->Format ("[disksrv] SetServiceStatus error %ld\n",status);
	}

	// This is where the service does its work.
	srv.Run();
	srv.Terminate();
	srv.Status.dwCurrentState       = SERVICE_STOPPED;
	srv.Status.dwCheckPoint         = 0;
	srv.Status.dwWaitHint           = 0;

	if (!SetServiceStatus (srv.StatusHandle, &srv.Status))
	{
		status = GetLastError();
		g_pLog->Format ("[disksrv] SetServiceStatus error %ld\n",status);
	}

	g_pLog->Format ("CDiskServer::ServiceMain: exit...\n");
	return; 
}

SERVICE_TABLE_ENTRY table[] = {
	{SERVICE_NAME, CDiskServer::ServiceMain},
	{NULL, NULL}
};

LPTSTR GetLastErrorText (LPTSTR lpszBuf, DWORD dwSize)
{
	DWORD dwRet;
	LPTSTR lpszTemp = NULL;

	dwRet = FormatMessage (
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |FORMAT_MESSAGE_ARGUMENT_ARRAY,
		NULL,
		GetLastError(),
		LANG_NEUTRAL,
		(LPTSTR)&lpszTemp,
		0,
		NULL);

	// supplied buffer is not long enough
	if (!dwRet || ((long)dwSize < (long)dwRet+14))
		lpszBuf[0] = TEXT('\0');
	else
	{
		lpszTemp[lstrlen(lpszTemp)-2] = TEXT('\0');  //remove cr and newline character
		_stprintf_s (lpszBuf, dwSize, TEXT("%s (0x%x)"), lpszTemp, GetLastError());
	}

	if ( lpszTemp )
		LocalFree((HLOCAL) lpszTemp );

	return lpszBuf;
}

VOID CmdInstallService()
{
	SC_HANDLE   schService;
	SC_HANDLE   schSCManager;

	TCHAR szPath[512];
	TCHAR szErr[256];

	if ( GetModuleFileName( NULL, szPath, 512 ) == 0 )
	{
		_tprintf(TEXT("Unable to install %s - %s\n"), TEXT(SERVICE_DISPLAY_NAME), GetLastErrorText(szErr, 256));
		return;
	}

	schSCManager = OpenSCManager(
		NULL,                   // machine (NULL == local)
		NULL,                   // database (NULL == default)
		SC_MANAGER_ALL_ACCESS   // access required
		);
	if ( schSCManager )
	{
		schService = CreateService(
			schSCManager,               // SCManager database
			TEXT(SERVICE_NAME),         // name of service
			TEXT(SERVICE_DISPLAY_NAME), // name to display
			SERVICE_ALL_ACCESS,         // desired access
			SERVICE_WIN32_OWN_PROCESS,  // service type
			SERVICE_DEMAND_START,       // start type
			SERVICE_ERROR_NORMAL,       // error control type
			szPath,                     // service's binary
			NULL,                       // no load ordering group
			NULL,                       // no tag identifier
			NULL,                       // dependencies
			NULL,                       // LocalSystem account
			NULL);                      // no password

		if ( schService )
		{
			_tprintf(TEXT("%s installed.\n"), TEXT(SERVICE_DISPLAY_NAME));
			CloseServiceHandle(schService);
		}
		else
		{
			_tprintf(TEXT("CreateService failed - %s\n"), GetLastErrorText(szErr, 256));
		}

		CloseServiceHandle(schSCManager);
	}
	else
		_tprintf(TEXT("OpenSCManager failed - %s\n"), GetLastErrorText(szErr,256));
}

VOID CmdRemoveService()
{
	SC_HANDLE   schService;
	SC_HANDLE   schSCManager;
	SERVICE_STATUS ssStatus;       // current status of the service
	TCHAR szErr[256];

	schSCManager = OpenSCManager(
		NULL,                   // machine (NULL == local)
		NULL,                   // database (NULL == default)
		SC_MANAGER_ALL_ACCESS   // access required
		);
	if ( schSCManager )
	{
		schService = OpenService(schSCManager, TEXT(SERVICE_NAME), SERVICE_ALL_ACCESS);

		if (schService)
		{
			// try to stop the service
			if (ControlService (schService, SERVICE_CONTROL_STOP, &ssStatus))
			{
				_tprintf(TEXT("Stopping %s."), TEXT(SERVICE_DISPLAY_NAME));
				Sleep( 1000 );

				while (QueryServiceStatus (schService, &ssStatus))
				{
					if (ssStatus.dwCurrentState == SERVICE_STOP_PENDING)
					{
						_tprintf(TEXT("."));
						Sleep( 1000 );
					}
					else
						break;
				}

				if (ssStatus.dwCurrentState == SERVICE_STOPPED)
					_tprintf(TEXT("\n%s stopped.\n"), TEXT(SERVICE_DISPLAY_NAME));
				else
					_tprintf(TEXT("\n%s failed to stop.\n"), TEXT(SERVICE_DISPLAY_NAME));

			}

			// now remove the service
			if( DeleteService(schService) )
				_tprintf(TEXT("%s removed.\n"), TEXT(SERVICE_DISPLAY_NAME));
			else
				_tprintf(TEXT("DeleteService failed - %s\n"), GetLastErrorText(szErr,256));


			CloseServiceHandle(schService);
		}
		else
			_tprintf(TEXT("OpenService failed - %s\n"), GetLastErrorText(szErr,256));

		CloseServiceHandle(schSCManager);
	}
	else
		_tprintf(TEXT("OpenSCManager failed - %s\n"), GetLastErrorText(szErr,256));
}

void __cdecl main (int ac, char *av[])
{
	OutputDebugString ("Disksrv: main entry\n");
	CRegKey rk;
	rk.Open (HKEY_LOCAL_MACHINE, (std::string("SYSTEM\\CurrentControlSet\\Services\\")
		+ std::string(SERVICE_NAME) + std::string("\\Parameters")).c_str());
	DWORD LogMask = 0;
	if (rk.QueryDWORDValue ("LogMask", LogMask) != ERROR_SUCCESS)
		LogMask = 0;
	InitLog(LogMask);
	if ( (ac > 1) &&
		((*av[1] == '-') || (*av[1] == '/')) )
	{
		OutputDebugString ("Disksrv: processing commands\n");
		if ( _stricmp( "install", av[1]+1 ) == 0 )
		{
			OutputDebugString ("Disksrv: calling CmdInstallService\n");
			CmdInstallService();
		}
		else if ( _stricmp( "remove", av[1]+1 ) == 0 )
		{
			OutputDebugString ("Disksrv: calling CmdRemoveService\n");
			CmdRemoveService();
		}
	}
	else
	{
		OutputDebugString ("Disksrv: calling StartServiceCtrlDispatcher\n");
		BOOL b = StartServiceCtrlDispatcher (table);
		OutputDebugString ("Disksrv: StartServiceCtrlDispatcher returned\n");
		if (!b)
		{
			DWORD dwErr = GetLastError();
			g_pLog->Format ("StartServiceCtrlDispatcher returned: dwErr = %d\n", dwErr);
		}
	}
	CloseLog();
}

extern "C" void __RPC_FAR * __RPC_API MIDL_user_allocate(size_t cBytes) 
{ 
    return(malloc(cBytes)); 
}

extern "C" void __RPC_API MIDL_user_free(void __RPC_FAR * p) 
{ 
    free(p); 
}

extern "C" error_status_t AddDisk (handle_t Binding,
								   ULONG diskId,
    /* [string][in] */ __RPC__in LPCWSTR diskType,
    /* [string][in] */ __RPC__in LPCWSTR diskName)
{
	RPC_STATUS status;

	g_pLog->Format ("RPC AddDisk: Binding = %08x, diskId = %d, diskType = \"%S\", diskName = \"%S\"\n",
		Binding, diskId, diskType, diskName);

	status = RpcImpersonateClient (Binding);
	g_pLog->Format ("RPC AddDisk: RpcImpersonateClient returned: %d\n", status);
	if (status != RPC_S_OK)
	{
		return(RPC_S_ACCESS_DENIED);
	}

	if (srv.AddDisk (diskId, diskType, diskName))
		status = RPC_S_OK;
	else
		status = GetLastError();
	RpcRevertToSelfEx (Binding);
	return status;
}

extern "C" error_status_t RemoveDisk (handle_t Binding,
									  ULONG diskId)
{
	RPC_STATUS status;

	g_pLog->Format ("RPC RemoveDisk: Binding = %08x, diskId = %d\n", Binding, diskId);
	status = RpcImpersonateClient (Binding);
	g_pLog->Format ("RPC AddDisk: RpcImpersonateClient returned: %d\n", status);
	if (status != RPC_S_OK)
	{
		return(RPC_S_ACCESS_DENIED);
	}

	srv.RemoveDisk (diskId);
	RpcRevertToSelfEx (Binding);
	return RPC_S_OK;
}
