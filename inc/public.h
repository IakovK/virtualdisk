/*++
Copyright (c) 1990-2000    Microsoft Corporation All Rights Reserved

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Author:
Environment:

    user and kernel
Notes:


Revision History:


--*/

//
// Define an Interface Guid for bus enumerator class.
// This GUID is used to register (IoRegisterDeviceInterface) 
// an instance of an interface so that enumerator application 
// can send an ioctl to the bus driver.
//

DEFINE_GUID(GUID_DEVINTERFACE_BUSENUM_VDISK, 
        0x16dd3662, 0x3b04, 0x493d, 0xbf, 0x32, 0xea, 0x8, 0x36, 0x30, 0x4b, 0xdf);
// {16DD3662-3B04-493d-BF32-EA0836304BDF}

//
// Define a Setup Class GUID for VDisk Class. This will go as BusTypeGuid in QueryBusInformation
//

DEFINE_GUID(GUID_DEVCLASS_VDISK, 
0x58855197, 0xf733, 0x41b6, 0xa3, 0x36, 0x1a, 0xcf, 0x53, 0x1d, 0x3b, 0x3);
// {58855197-F733-41b6-A336-1ACF531D3B03}

//
// Define a WMI GUID to get busenum info.
//

DEFINE_GUID(VDISK_BUS_WMI_STD_DATA_GUID, 
0xa2a7a742, 0x4a97, 0x4dbf, 0x86, 0x4f, 0x64, 0x6b, 0xb8, 0xa7, 0x62, 0x16);
// {A2A7A742-4A97-4dbf-864F-646BB8A76216}

//
// GUID definition are required to be outside of header inclusion pragma to avoid
// error during precompiled headers.
//

#ifndef __PUBLIC_H
#define __PUBLIC_H

#define BUFFER_SIZE 256 * 1024
#define BUFFER_ALIGNMENT PAGE_SIZE

#define FILE_DEVICE_BUSENUM         FILE_DEVICE_BUS_EXTENDER

#define BUSENUM_IOCTL(_index_) \
    CTL_CODE (FILE_DEVICE_BUSENUM, _index_, METHOD_BUFFERED, FILE_READ_DATA)

#define IOCTL_BUSENUM_ADD_DISK       BUSENUM_IOCTL (0x0)
#define IOCTL_BUSENUM_REMOVE_DISK    BUSENUM_IOCTL (0x1)
#define IOCTL_BUSENUM_GET_REQUEST    BUSENUM_IOCTL (0x2)
#define IOCTL_BUSENUM_SEND_REPLY     BUSENUM_IOCTL (0x3)

typedef struct _BUSENUM_ADD_DISK
{
	ULONG handle;		// opaque handle
	ULONG nBlocks;		// file size in blocks;
	ULONG BlockSize;	// block size in bytes;
	PVOID buffer;		// buffer for disk data; must be BUFFERSIZE long and page aligned
	HANDLE RemoveEvent;	// event that is signaled by driver when the PDO is removed and waited by service to remove user-mode PDO
} BUSENUM_ADD_DISK, *PBUSENUM_ADD_DISK;

typedef struct _BUSENUM_REMOVE_DISK
{
	ULONG handle;		// opaque handle
} BUSENUM_REMOVE_DISK, *PBUSENUM_REMOVE_DISK;

// if request is SCSIOP_WRITE, data is available in pdoData->Buffer
// and is expected to be transferred to external storage
typedef struct _BUSENUM_REQUEST
{
	ULONG handle;				// handle to device
	ULONG code;					// read/write
	ULONG logicalBlockAddress;	// starting logicall block for operation
	ULONG transferBlocks;		// count of logical blocks to transfer
} BUSENUM_REQUEST, *PBUSENUM_REQUEST;

// if request is SCSIOP_READ, data is available in pdoData->Buffer
typedef struct _BUSENUM_REPLY
{
	ULONG handle;		// handle to device
	NTSTATUS status;	// status of operation
	ULONG code;			// read/write (code of operation completed)
	ULONG numBytes;		// number of bytes transferred
} BUSENUM_REPLY, *PBUSENUM_REPLY;

#endif
