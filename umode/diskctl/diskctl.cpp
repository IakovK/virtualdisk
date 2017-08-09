#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <rpc.h>

#include "disksrv.h"

class CRPCWrapper
{
	unsigned char * m_pszStringBinding;
	RPC_BINDING_HANDLE m_binding;
public:
	RPC_STATUS Init (unsigned char * pszProtocolSequence, unsigned char * pszEndpoint);
	~CRPCWrapper();
	RPC_STATUS AddDisk (ULONG diskId, LPCWSTR diskType, LPCWSTR diskName);
	RPC_STATUS RemoveDisk (ULONG diskId);
};

CRPCWrapper::~CRPCWrapper()
{
	RpcStringFree(&m_pszStringBinding);
	RpcBindingFree(&m_binding);
}

RPC_STATUS CRPCWrapper::Init (unsigned char * pszProtocolSequence, unsigned char * pszEndpoint)
{
	RPC_STATUS status;
	status = RpcStringBindingCompose (
		NULL,
		pszProtocolSequence,
		NULL,
		pszEndpoint,
		NULL,
		&m_pszStringBinding);
	if (status)
	{
		return status;
	}
	status = RpcBindingFromStringBinding (
		m_pszStringBinding,
		&m_binding);
	return status;
}

RPC_STATUS CRPCWrapper::AddDisk (ULONG diskId, LPCWSTR diskType, LPCWSTR diskName)
{
	DWORD nBytes = GetFullPathNameW (diskName, 0, NULL, NULL) * sizeof (WCHAR);
	LPWSTR fullName = (LPWSTR)_alloca (nBytes);
	nBytes = GetFullPathNameW (diskName, nBytes/2, fullName, NULL);
	return ::AddDisk (m_binding, diskId, diskType, fullName);
}

RPC_STATUS CRPCWrapper::RemoveDisk (ULONG diskId)
{
	return ::RemoveDisk (m_binding, diskId);
}

void __cdecl wmain (int ac, wchar_t *av[])
{
	CRPCWrapper rpcw;
	unsigned char * pszProtocolSequence = (unsigned char *)"ncalrpc";
	unsigned char * pszEndpoint    = (unsigned char *)"disksrv";

	RPC_STATUS status = rpcw.Init ((unsigned char *)"ncalrpc", (unsigned char *)"disksrv");
	if (status)
	{
		printf ("rpcw.Init returned: %d\n", status);
		return;
	}
	if (ac < 2)
	{
		printf ("usage: diskctl add <args> or diskctl remove <args>\n");
		return;
	}
	if (wcscmp (av[1], L"add") == 0)
	{
		if (ac < 5)
		{
			printf ("usage: diskctl add <disk num> <disk type> <disk name>\n");
			return;
		}
		ULONG diskId = _wtol (av[2]);
		LPWSTR diskType = av[3];
		LPWSTR diskName = av[4];
		RPC_STATUS s = rpcw.AddDisk (diskId, diskType, diskName);
		if (s == RPC_S_OK)
			printf ("disk %d added\n", diskId);
		else
			printf ("failed to add disk: errcode = %d\n", s);
	}
	else if (wcscmp (av[1], L"remove") == 0)
	{
		if (ac < 3)
		{
			printf ("usage: diskctl remove <disk num>\n");
			return;
		}
		ULONG diskId = _wtol (av[2]);
		RPC_STATUS s = rpcw.RemoveDisk (diskId);
		if (s == RPC_S_OK)
			printf ("disk %d removed\n", diskId);
		else
			printf ("failed to remove disk: errcode = %d\n", s);
	}
	else
	{
		printf ("usage: diskctl add <args> or diskctl remove <args>\n");
		return;
	}
}

extern "C" void __RPC_FAR * __RPC_API MIDL_user_allocate(size_t cBytes)
{ 
    return malloc(cBytes);
}

extern "C" void __RPC_API MIDL_user_free(void __RPC_FAR * p)
{ 
    free(p);
}
