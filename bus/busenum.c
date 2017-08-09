/*++

Copyright (c) 1990-2000  Microsoft Corporation All Rights Reserved

Module Name:

    BUSENUM.C

Abstract:

    This module contains the entry points for a vdisk bus driver.

Author:


Environment:

    kernel mode only

Revision History:

    Cleaned up sample 05/05/99
    Fixed the create_close and ioctl handler to fail the request 
    sent on the child stack - 3/15/04


--*/

#include "busenum.h"


//
// Global Debug Level
//

ULONG BusEnumDebugLevel = BUS_DEFAULT_DEBUG_OUTPUT_LEVEL;


GLOBALS Globals;


#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, Bus_DriverUnload)
#pragma alloc_text (PAGE, Bus_CreateClose)
#pragma alloc_text (PAGE, Bus_Cleanup)
#pragma alloc_text (PAGE, Bus_IoCtl)
#pragma alloc_text (PAGE, Bus_FDO_IoCtl)
#pragma alloc_text (PAGE, Bus_PDO_IoCtl)
#endif

void SetSenseInfo (PPDO_DEVICE_DATA pdoData, PVOID Buffer, UCHAR senseCode, UCHAR asc, UCHAR asq);

DRIVER_CANCEL Bus_CancelIrp;

#if DBG
#define CASE_STRING(x) case x:return #x;
char *ModeSensePageCodeToString(UCHAR PageCode)
{
    switch (PageCode)
    {
    default: return "Unknown";
    CASE_STRING(MODE_PAGE_ERROR_RECOVERY)
    CASE_STRING(MODE_PAGE_DISCONNECT)
    CASE_STRING(MODE_PAGE_FORMAT_DEVICE)
    CASE_STRING(MODE_PAGE_RIGID_GEOMETRY)
    CASE_STRING(MODE_PAGE_FLEXIBILE)
    CASE_STRING(MODE_PAGE_VERIFY_ERROR)
    CASE_STRING(MODE_PAGE_CACHING)
    CASE_STRING(MODE_PAGE_PERIPHERAL)
    CASE_STRING(MODE_PAGE_CONTROL)
    CASE_STRING(MODE_PAGE_MEDIUM_TYPES)
    CASE_STRING(MODE_PAGE_NOTCH_PARTITION)
    CASE_STRING(MODE_PAGE_CD_AUDIO_CONTROL)
    CASE_STRING(MODE_PAGE_DATA_COMPRESS)
    CASE_STRING(MODE_PAGE_DEVICE_CONFIG)
    CASE_STRING(MODE_PAGE_MEDIUM_PARTITION)
    CASE_STRING(MODE_PAGE_CDVD_FEATURE_SET)
    CASE_STRING(MODE_PAGE_POWER_CONDITION)
    CASE_STRING(MODE_PAGE_FAULT_REPORTING)
    CASE_STRING(MODE_PAGE_ELEMENT_ADDRESS)
    CASE_STRING(MODE_PAGE_TRANSPORT_GEOMETRY)
    CASE_STRING(MODE_PAGE_DEVICE_CAPABILITIES)
    CASE_STRING(MODE_PAGE_CAPABILITIES)
    }
}

char *SrbFunctionToString (UCHAR Function)
{
    switch (Function)
    {
    default: return "Unknown";
    CASE_STRING(SRB_FUNCTION_EXECUTE_SCSI)
    CASE_STRING(SRB_FUNCTION_CLAIM_DEVICE)
    CASE_STRING(SRB_FUNCTION_IO_CONTROL)
    CASE_STRING(SRB_FUNCTION_RECEIVE_EVENT)
    CASE_STRING(SRB_FUNCTION_RELEASE_QUEUE)
    CASE_STRING(SRB_FUNCTION_ATTACH_DEVICE)
    CASE_STRING(SRB_FUNCTION_RELEASE_DEVICE)
    CASE_STRING(SRB_FUNCTION_SHUTDOWN)
    CASE_STRING(SRB_FUNCTION_FLUSH)
    CASE_STRING(SRB_FUNCTION_ABORT_COMMAND)
    CASE_STRING(SRB_FUNCTION_RELEASE_RECOVERY)
    CASE_STRING(SRB_FUNCTION_RESET_BUS)
    CASE_STRING(SRB_FUNCTION_RESET_DEVICE)
    CASE_STRING(SRB_FUNCTION_TERMINATE_IO)
    CASE_STRING(SRB_FUNCTION_FLUSH_QUEUE)
    CASE_STRING(SRB_FUNCTION_REMOVE_DEVICE)
    CASE_STRING(SRB_FUNCTION_WMI)
    CASE_STRING(SRB_FUNCTION_LOCK_QUEUE)
    CASE_STRING(SRB_FUNCTION_UNLOCK_QUEUE)
    }
}

char *SCSICommandToString (UCHAR cmd)
{
    switch (cmd)
    {
    default: return "Unknown";
    CASE_STRING(SCSIOP_TEST_UNIT_READY)
    CASE_STRING(SCSIOP_REWIND)
    CASE_STRING(SCSIOP_REQUEST_BLOCK_ADDR)
    CASE_STRING(SCSIOP_REQUEST_SENSE)
    CASE_STRING(SCSIOP_FORMAT_UNIT)
    CASE_STRING(SCSIOP_READ_BLOCK_LIMITS)
    CASE_STRING(SCSIOP_REASSIGN_BLOCKS)
    CASE_STRING(SCSIOP_READ6)
    CASE_STRING(SCSIOP_WRITE6)
    CASE_STRING(SCSIOP_SEEK6)
    CASE_STRING(SCSIOP_SEEK_BLOCK)
    CASE_STRING(SCSIOP_PARTITION)
    CASE_STRING(SCSIOP_READ_REVERSE)
    CASE_STRING(SCSIOP_WRITE_FILEMARKS)
    CASE_STRING(SCSIOP_SPACE)
    CASE_STRING(SCSIOP_INQUIRY)
    CASE_STRING(SCSIOP_VERIFY6)
    CASE_STRING(SCSIOP_RECOVER_BUF_DATA)
    CASE_STRING(SCSIOP_MODE_SELECT)
    CASE_STRING(SCSIOP_RESERVE_UNIT)
    CASE_STRING(SCSIOP_RELEASE_UNIT)

    CASE_STRING(SCSIOP_COPY)
    CASE_STRING(SCSIOP_ERASE)
    CASE_STRING(SCSIOP_MODE_SENSE)
    CASE_STRING(SCSIOP_LOAD_UNLOAD)
    CASE_STRING(SCSIOP_RECEIVE_DIAGNOSTIC)
    CASE_STRING(SCSIOP_SEND_DIAGNOSTIC)
    CASE_STRING(SCSIOP_MEDIUM_REMOVAL)
    CASE_STRING(SCSIOP_READ_FORMATTED_CAPACITY)
    CASE_STRING(SCSIOP_READ_CAPACITY)
    CASE_STRING(SCSIOP_READ)
    CASE_STRING(SCSIOP_WRITE)
    CASE_STRING(SCSIOP_SEEK)
    CASE_STRING(SCSIOP_WRITE_VERIFY)
    CASE_STRING(SCSIOP_VERIFY)
    CASE_STRING(SCSIOP_SEARCH_DATA_HIGH)
    CASE_STRING(SCSIOP_SEARCH_DATA_EQUAL)
    CASE_STRING(SCSIOP_SEARCH_DATA_LOW)
    CASE_STRING(SCSIOP_SET_LIMITS)
    CASE_STRING(SCSIOP_READ_POSITION)
    CASE_STRING(SCSIOP_SYNCHRONIZE_CACHE)
    CASE_STRING(SCSIOP_COMPARE)
    CASE_STRING(SCSIOP_COPY_COMPARE)
    CASE_STRING(SCSIOP_WRITE_DATA_BUFF)
    CASE_STRING(SCSIOP_READ_DATA_BUFF)
    CASE_STRING(SCSIOP_CHANGE_DEFINITION)
    CASE_STRING(SCSIOP_READ_SUB_CHANNEL)
    CASE_STRING(SCSIOP_READ_TOC)
    CASE_STRING(SCSIOP_READ_HEADER)
    CASE_STRING(SCSIOP_PLAY_AUDIO)
    CASE_STRING(SCSIOP_PLAY_AUDIO_MSF)
    CASE_STRING(SCSIOP_PLAY_TRACK_INDEX)
    CASE_STRING(SCSIOP_PLAY_TRACK_RELATIVE)
    CASE_STRING(SCSIOP_PAUSE_RESUME)
    CASE_STRING(SCSIOP_LOG_SELECT)
    CASE_STRING(SCSIOP_LOG_SENSE)
    CASE_STRING(SCSIOP_STOP_PLAY_SCAN)
    CASE_STRING(SCSIOP_READ_DISK_INFORMATION)
    CASE_STRING(SCSIOP_READ_TRACK_INFORMATION)
    CASE_STRING(SCSIOP_MODE_SELECT10)
    CASE_STRING(SCSIOP_MODE_SENSE10)
    CASE_STRING(SCSIOP_REPORT_LUNS)
    CASE_STRING(SCSIOP_SEND_KEY)
    CASE_STRING(SCSIOP_REPORT_KEY)
    CASE_STRING(SCSIOP_MOVE_MEDIUM)
    CASE_STRING(SCSIOP_EXCHANGE_MEDIUM)
    CASE_STRING(SCSIOP_SET_READ_AHEAD)
    CASE_STRING(SCSIOP_READ_DVD_STRUCTURE)
    CASE_STRING(SCSIOP_REQUEST_VOL_ELEMENT)
    CASE_STRING(SCSIOP_SEND_VOLUME_TAG)
    CASE_STRING(SCSIOP_READ_ELEMENT_STATUS)
    CASE_STRING(SCSIOP_READ_CD_MSF)
    CASE_STRING(SCSIOP_SCAN_CD)
    CASE_STRING(SCSIOP_PLAY_CD)
    CASE_STRING(SCSIOP_MECHANISM_STATUS)
    CASE_STRING(SCSIOP_READ_CD)
    CASE_STRING(SCSIOP_INIT_ELEMENT_RANGE)
    }
}
#endif

NTSTATUS
DriverEntry (
    __in  PDRIVER_OBJECT  DriverObject,
    __in  PUNICODE_STRING RegistryPath
    )
/*++
Routine Description:

    Initialize the driver dispatch table.

Arguments:

    DriverObject - pointer to the driver object

    RegistryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:

  NT Status Code

--*/
{

    Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Driver Entry \n"));

    //
    // Save the RegistryPath for WMI.
    //

    Globals.RegistryPath.MaximumLength = RegistryPath->Length +
                                          sizeof(UNICODE_NULL);
    Globals.RegistryPath.Length = RegistryPath->Length;
    Globals.RegistryPath.Buffer = ExAllocatePoolWithTag(
                                       PagedPool,
                                       Globals.RegistryPath.MaximumLength,
                                       BUSENUM_POOL_TAG
                                       );

    if (!Globals.RegistryPath.Buffer) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyUnicodeString(&Globals.RegistryPath, RegistryPath);

    //
    // Set entry points into the driver
    //
    DriverObject->MajorFunction [IRP_MJ_CREATE] =
    DriverObject->MajorFunction [IRP_MJ_CLOSE] = Bus_CreateClose;
    DriverObject->MajorFunction [IRP_MJ_CLEANUP] = Bus_Cleanup;
    DriverObject->MajorFunction [IRP_MJ_PNP] = Bus_PnP;
    DriverObject->MajorFunction [IRP_MJ_POWER] = Bus_Power;
    DriverObject->MajorFunction [IRP_MJ_DEVICE_CONTROL] = Bus_IoCtl;
    DriverObject->MajorFunction [IRP_MJ_SCSI] = Bus_Scsi;
    DriverObject->DriverUnload = Bus_DriverUnload;
    DriverObject->DriverExtension->AddDevice = Bus_AddDevice;

    return STATUS_SUCCESS;
}

NTSTATUS
Bus_FDO_Cleanup (
    __in  PDEVICE_OBJECT  DeviceObject,
    __in  PIRP            Irp
    )
{
	PIO_STACK_LOCATION irpSp;
	NTSTATUS status = STATUS_SUCCESS;
    PIRP pendingIrp;

	PFDO_DEVICE_DATA fdoData = (PFDO_DEVICE_DATA)DeviceObject->DeviceExtension;
	if (fdoData->DevicePnPState == Deleted)
	{
		Irp->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		return status;
	}
    fdoData = (PFDO_DEVICE_DATA) DeviceObject->DeviceExtension;

	// complete all pending IRPs and delete all disk devices
	Bus_KdPrint(fdoData, BUS_DBG_SS_TRACE, ("Bus_FDO_Cleanup: DeviceObject = %p, Irp = %p\n", DeviceObject, Irp));

	irpSp = IoGetCurrentIrpStackLocation(Irp);

	while(pendingIrp = IoCsqRemoveNextIrp(&fdoData->RequestQueue.CancelSafeQueue,
		irpSp->FileObject))
	{

		//
		// Cancel the IRP
		//

		Bus_KdPrint(fdoData, BUS_DBG_SS_TRACE, ("Bus_FDO_Cleanup: pendingIrp = %p\n", pendingIrp));
		IoSetCancelRoutine (pendingIrp, NULL);
		pendingIrp->IoStatus.Information = 0;
		pendingIrp->IoStatus.Status = STATUS_CANCELLED;
		IoCompleteRequest(pendingIrp, IO_NO_INCREMENT);
	}

	if (fdoData->NumPDOs > 0)
	{
		KeResetEvent (&fdoData->CleanupReadyEvent);
		Bus_KdPrint(fdoData, BUS_DBG_SS_TRACE, ("Bus_FDO_Cleanup: calling Bus_RemoveAllDisks\n"));
		Bus_RemoveAllDisks (fdoData);
		Bus_KdPrint(fdoData, BUS_DBG_SS_TRACE, ("Bus_FDO_Cleanup: calling Bus_RemoveAllDisks done, wait for removal last disk\n"));
		KeWaitForSingleObject (&fdoData->CleanupReadyEvent, UserRequest, KernelMode, TRUE, NULL);
		Bus_KdPrint(fdoData, BUS_DBG_SS_TRACE, ("Bus_FDO_Cleanup: wait for removal last disk done\n"));
	}

	Bus_KdPrint(fdoData, BUS_DBG_SS_TRACE, ("Bus_FDO_Cleanup: exit\n"));
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS
Bus_PDO_Cleanup (
    __in  PDEVICE_OBJECT  DeviceObject,
    __in  PIRP            Irp
    )
{
	NTSTATUS status = STATUS_SUCCESS;

	PPDO_DEVICE_DATA pdoData = (PPDO_DEVICE_DATA)DeviceObject->DeviceExtension;
	if (!pdoData->Present)
	{
		Irp->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		return status;
	}
	else
	{
		Irp->IoStatus.Status = status = STATUS_SUCCESS;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		return status;
	}
}

NTSTATUS
Bus_Cleanup (
    __in  PDEVICE_OBJECT  DeviceObject,
    __in  PIRP            Irp
    )
{
    PCOMMON_DEVICE_DATA     commonData;

    PAGED_CODE ();

    commonData = (PCOMMON_DEVICE_DATA) DeviceObject->DeviceExtension;

	if (commonData->IsFDO)
	{
		return Bus_FDO_Cleanup (DeviceObject, Irp);
	}
	else
	{
		return Bus_PDO_Cleanup (DeviceObject, Irp);
	}

}

NTSTATUS
Bus_CreateClose (
    __in  PDEVICE_OBJECT  DeviceObject,
    __in  PIRP            Irp
    )
{
    PIO_STACK_LOCATION  irpStack;
    NTSTATUS            status;
    PFDO_DEVICE_DATA    fdoData;
    PCOMMON_DEVICE_DATA     commonData;

    PAGED_CODE ();

    commonData = (PCOMMON_DEVICE_DATA)DeviceObject->DeviceExtension;

    status = IoAcquireRemoveLock (&commonData->RemoveLock, Irp);
    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);

        return status;
    }

    //
    // Check to see whether the bus is removed
    //

	if (commonData->IsFDO)
	{
		PFDO_DEVICE_DATA fdoData = (PFDO_DEVICE_DATA)DeviceObject->DeviceExtension;
		if (fdoData->DevicePnPState == Deleted)
		{
			Irp->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE;
			IoCompleteRequest (Irp, IO_NO_INCREMENT);
			return status;
		}
	}
	else
	{
		PPDO_DEVICE_DATA pdoData = (PPDO_DEVICE_DATA)DeviceObject->DeviceExtension;
		if (!pdoData->Present)
		{
			Irp->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE;
			IoCompleteRequest (Irp, IO_NO_INCREMENT);
			return status;
		}
	}


    irpStack = IoGetCurrentIrpStackLocation (Irp);

    switch (irpStack->MajorFunction) {
    case IRP_MJ_CREATE:
		if (commonData->IsFDO)
		{
			Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Create FDO\n"));
		}
		else
		{
			Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Create PDO\n"));
		}
        status = STATUS_SUCCESS;
        break;

    case IRP_MJ_CLOSE:
		if (commonData->IsFDO)
		{
			Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Close FDO\n"));
		}
		else
		{
			Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Close PDO\n"));
		}
        status = STATUS_SUCCESS;
        break;
     default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock (&commonData->RemoveLock, Irp);
    return status;
}

NTSTATUS
Bus_PDO_IoCtl (
    __in PDEVICE_OBJECT       DeviceObject,
    __in PIRP                 Irp,
    __in PIO_STACK_LOCATION   irpStack,
    __in PPDO_DEVICE_DATA     DeviceData
    )
{
    NTSTATUS                status;

    PAGED_CODE ();

	status = IoAcquireRemoveLock (&DeviceData->RemoveLock, Irp);
    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);

        return status;
    }

    status = STATUS_INVALID_PARAMETER;

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_STORAGE_QUERY_PROPERTY:
        if (irpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(STORAGE_PROPERTY_QUERY))	// invalid buffer size
			break;
		{
			PSTORAGE_PROPERTY_QUERY pstQuery = (PSTORAGE_PROPERTY_QUERY)Irp->AssociatedIrp.SystemBuffer;
			Bus_KdPrint(DeviceData, BUS_DBG_IOCTL_TRACE, ("Bus_PDO_IoCtl(IOCTL_STORAGE_QUERY_PROPERTY):"
				" DeviceObject = %p, QueryType = %d, PropertyId = %d, OutputBufferLength = %d\n",
				DeviceObject, pstQuery->QueryType, pstQuery->PropertyId,
				irpStack->Parameters.DeviceIoControl.OutputBufferLength));
			if (pstQuery->QueryType == PropertyStandardQuery && pstQuery->PropertyId == StorageAdapterProperty)
			{
				PSTORAGE_DESCRIPTOR_HEADER psdh;
				PSTORAGE_ADAPTER_DESCRIPTOR psad;
		        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
					sizeof (STORAGE_DESCRIPTOR_HEADER))	// invalid buffer size
					break;
				if (irpStack->Parameters.DeviceIoControl.OutputBufferLength < 
					sizeof (STORAGE_ADAPTER_DESCRIPTOR))
				{
					psdh = (PSTORAGE_DESCRIPTOR_HEADER)Irp->AssociatedIrp.SystemBuffer;
					psdh->Version = sizeof (STORAGE_ADAPTER_DESCRIPTOR);
					psdh->Size = sizeof (STORAGE_ADAPTER_DESCRIPTOR);
					status = STATUS_SUCCESS;
					Irp->IoStatus.Information = sizeof (STORAGE_DESCRIPTOR_HEADER);
					break;
				}
				psad = (PSTORAGE_ADAPTER_DESCRIPTOR)Irp->AssociatedIrp.SystemBuffer;
				psad->Version = sizeof (STORAGE_ADAPTER_DESCRIPTOR);
				psad->Size = sizeof (STORAGE_ADAPTER_DESCRIPTOR);
				psad->MaximumTransferLength = BUFFER_SIZE;
				psad->MaximumPhysicalPages = -1;
				psad->AlignmentMask = 0;
				psad->AdapterUsesPio = TRUE;
				psad->AdapterScansDown = FALSE;
				psad->CommandQueueing = FALSE;
				psad->AcceleratedTransfer = FALSE;
				psad->BusType = BusTypeUnknown;
				psad->BusMajorVersion = 1;
				psad->BusMinorVersion = 1;
				status = STATUS_SUCCESS;
				Irp->IoStatus.Information = sizeof (STORAGE_ADAPTER_DESCRIPTOR);
				break;
			}
			if (pstQuery->QueryType == PropertyStandardQuery && pstQuery->PropertyId == StorageDeviceProperty)
			{
				PSTORAGE_DESCRIPTOR_HEADER psdh;
				PSTORAGE_DEVICE_DESCRIPTOR psdd;
		        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
					sizeof (STORAGE_DESCRIPTOR_HEADER))	// invalid buffer size
					break;
				if (irpStack->Parameters.DeviceIoControl.OutputBufferLength < 
					sizeof (STORAGE_DEVICE_DESCRIPTOR))
				{
					psdh = (PSTORAGE_DESCRIPTOR_HEADER)Irp->AssociatedIrp.SystemBuffer;
					psdh->Version = sizeof (STORAGE_DEVICE_DESCRIPTOR);
					psdh->Size = sizeof (STORAGE_DEVICE_DESCRIPTOR);
					status = STATUS_SUCCESS;
					Irp->IoStatus.Information = sizeof (STORAGE_DESCRIPTOR_HEADER);
					break;
				}
				psdd = (PSTORAGE_DEVICE_DESCRIPTOR)Irp->AssociatedIrp.SystemBuffer;
				psdd->Version = sizeof (STORAGE_DEVICE_DESCRIPTOR);
				psdd->Size = sizeof (STORAGE_DEVICE_DESCRIPTOR);
				psdd->DeviceType = DIRECT_ACCESS_DEVICE;
				psdd->DeviceTypeModifier = 0;
				psdd->RemovableMedia = FALSE;
				psdd->CommandQueueing = FALSE;
				psdd->VendorIdOffset = 0;
				psdd->ProductIdOffset = 0;
				psdd->ProductRevisionOffset = 0;
				psdd->SerialNumberOffset = -1;
				psdd->BusType = BusTypeUnknown;
				psdd->RawPropertiesLength = 0;
				psdd->RawDeviceProperties[0] = 0;
				status = STATUS_SUCCESS;
				Irp->IoStatus.Information = sizeof (STORAGE_ADAPTER_DESCRIPTOR);
				break;
			}
		}
        break;

    case IOCTL_DISK_GET_DRIVE_GEOMETRY:
		Bus_KdPrint(DeviceData, BUS_DBG_IOCTL_TRACE, ("Bus_PDO_IoCtl(IOCTL_DISK_GET_DRIVE_GEOMETRY)\n"));
		if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
			sizeof (DISK_GEOMETRY))	// invalid buffer size
			break;
		{
			PDISK_GEOMETRY dg = (PDISK_GEOMETRY)Irp->AssociatedIrp.SystemBuffer;
			LARGE_INTEGER diskSize;
			diskSize.QuadPart = DeviceData->BlockSize * DeviceData->nBlocks;
			dg->TracksPerCylinder = 255;
			dg->SectorsPerTrack = 63;
			dg->BytesPerSector = 512;
			dg->Cylinders.QuadPart = diskSize.QuadPart / (512 * 255 * 63);
			dg->MediaType = FixedMedia;
			status = STATUS_SUCCESS;
			Irp->IoStatus.Information = sizeof (DISK_GEOMETRY);
		}
		break;
    case IOCTL_SCSI_GET_ADDRESS:
		Bus_KdPrint(DeviceData, BUS_DBG_IOCTL_TRACE, ("Bus_PDO_IoCtl(IOCTL_SCSI_GET_ADDRESS)\n"));
		if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
			sizeof (SCSI_ADDRESS))	// invalid buffer size
			break;
		{
			PSCSI_ADDRESS addr = (PSCSI_ADDRESS)Irp->AssociatedIrp.SystemBuffer;
			PUCHAR ptr = (PUCHAR)&DeviceData->SerialNo;
			addr->Length = sizeof (SCSI_ADDRESS);
			addr->PortNumber = ptr[3];
			addr->PathId = ptr[2];
			addr->TargetId = ptr[1];
			addr->Lun = ptr[0];
			status = STATUS_SUCCESS;
			Irp->IoStatus.Information = sizeof (SCSI_ADDRESS);
		}
		break;
    default:
		Bus_KdPrint(DeviceData, BUS_DBG_IOCTL_TRACE, ("Bus_PDO_IoCtl(%08x)\n",
			irpStack->Parameters.DeviceIoControl.IoControlCode));
        break; // default status is STATUS_INVALID_PARAMETER
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock (&DeviceData->RemoveLock, Irp);
    return status;
}

void BusSetupRequest (PIRP rqIrp, PPDO_DEVICE_DATA pdoData, PIRP Irp)
{
	PBUSENUM_REQUEST rq = (PBUSENUM_REQUEST)rqIrp->AssociatedIrp.SystemBuffer;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation (Irp);
    PSCSI_REQUEST_BLOCK srb = irpStack->Parameters.Scsi.Srb;
	USHORT transferBlocks;
	ULONG logicalBlockAddress;
	ULONG_PTR offset;
	PVOID buffer;

	PCDB pcdb = (PCDB)srb->Cdb;
	rq->handle = pdoData->SerialNo;

	Bus_KdPrint(pdoData, BUS_DBG_SCSI_TRACE,
		("BusSetupRequest: Irp = %p, irpStack = %p, "
		"srb = %p\n", Irp, irpStack, srb));

	offset = (PCHAR)srb->DataBuffer - (PCHAR)MmGetMdlVirtualAddress(Irp->MdlAddress);
	buffer = (PCHAR)MmGetSystemAddressForMdlSafe(Irp->MdlAddress,HighPagePriority) + offset;

	//big-endian --> little-endian format
	((PFOUR_BYTE)&logicalBlockAddress)->Byte3 = pcdb->CDB10.LogicalBlockByte0;
	((PFOUR_BYTE)&logicalBlockAddress)->Byte2 = pcdb->CDB10.LogicalBlockByte1;
	((PFOUR_BYTE)&logicalBlockAddress)->Byte1 = pcdb->CDB10.LogicalBlockByte2;
	((PFOUR_BYTE)&logicalBlockAddress)->Byte0 = pcdb->CDB10.LogicalBlockByte3;

	((PTWO_BYTE)&transferBlocks)->Byte1 = pcdb->CDB10.TransferBlocksMsb;
	((PTWO_BYTE)&transferBlocks)->Byte0 = pcdb->CDB10.TransferBlocksLsb;

	Bus_KdPrint(pdoData, BUS_DBG_SCSI_NOISE,
		("BusSetupRequest: OperationCode = %02x, TransferBlocksMsb = %02x, "
		"TransferBlocksLsb = %02x, transferBlocks = %04x\n",
		pcdb->CDB6GENERIC.OperationCode,
		pcdb->CDB10.TransferBlocksMsb, pcdb->CDB10.TransferBlocksLsb, transferBlocks));
	Bus_KdPrint(pdoData, BUS_DBG_SCSI_NOISE,
		("BusSetupRequest: LogicalBlockByte0..3 = (%02x, %02x, %02x, %02x), logicalBlockAddress = %08x\n",
		pcdb->CDB10.LogicalBlockByte0, pcdb->CDB10.LogicalBlockByte1,
		pcdb->CDB10.LogicalBlockByte2, pcdb->CDB10.LogicalBlockByte3, logicalBlockAddress));
	if (pcdb->CDB6GENERIC.OperationCode == SCSIOP_WRITE)	// write data to pdodata->buffer
	{
		RtlCopyMemory (pdoData->buffer, buffer, srb->DataTransferLength);
	}

	rq->code = pcdb->CDB6GENERIC.OperationCode;
	rq->logicalBlockAddress = logicalBlockAddress;
	rq->transferBlocks = transferBlocks;

	rqIrp->IoStatus.Status = STATUS_SUCCESS;
	rqIrp->IoStatus.Information = sizeof (BUSENUM_REQUEST);
}

VOID
Bus_CancelIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
	IoReleaseCancelSpinLock(Irp->CancelIrql);
	Irp->IoStatus.Status = STATUS_CANCELLED;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
}

void BusStartNextPacket (PPDO_DEVICE_DATA pdoData)
{
	KIRQL  OldIrql;
	NTSTATUS status;

	Bus_KdPrint(pdoData, BUS_DBG_COMM_TRACE, ("BusStartNextPacket: pdoData = %p, SerialNo = %08x\n",
		pdoData, pdoData->SerialNo));
	KeAcquireSpinLock (&pdoData->PDOLock, &OldIrql);
	Bus_KdPrint(pdoData, BUS_DBG_COMM_TRACE, ("BusStartNextPacket: pdoData->CurrentIrp = %p\n",
		pdoData->CurrentIrp));
	if (pdoData->CurrentIrp == NULL)
	{
		PDEVICE_OBJECT parentFDO = pdoData->ParentFdo;
	    PFDO_DEVICE_DATA fdoData = (PFDO_DEVICE_DATA) parentFDO->DeviceExtension;
		PIRP rqIrp = IoCsqRemoveNextIrp (&fdoData->RequestQueue.CancelSafeQueue, NULL);
		Bus_KdPrint(pdoData, BUS_DBG_COMM_TRACE, ("BusStartNextPacket: rqIrp = %p\n",
			rqIrp));
		if (rqIrp == NULL)
		{
			KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
		}
		else
		{
			PIRP Irp = IoCsqRemoveNextIrp (&pdoData->IrpQueue.CancelSafeQueue, NULL);
			Bus_KdPrint(pdoData, BUS_DBG_COMM_TRACE, ("BusStartNextPacket: Irp = %p\n",
				Irp));
			if (Irp != NULL)
			{
			    status = IoAcquireRemoveLock (&pdoData->RemoveLock, Irp);
				if (!NT_SUCCESS(status))
				{
					IoCsqInsertIrp (&fdoData->RequestQueue.CancelSafeQueue, rqIrp, NULL);
					KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
					Irp->IoStatus.Information = 0;
					Irp->IoStatus.Status = status;
					IoCompleteRequest(Irp, IO_NO_INCREMENT);
					return;
				}
				BusSetupRequest (rqIrp, pdoData, Irp);
				pdoData->CurrentIrp = Irp;
				IoMarkIrpPending (Irp);
				IoSetCancelRoutine(Irp, Bus_CancelIrp);
				KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
				IoCompleteRequest(rqIrp, IO_NO_INCREMENT);
			}
			else
			{
				IoCsqInsertIrp (&fdoData->RequestQueue.CancelSafeQueue, rqIrp, NULL);
				KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
			}
		}
	}
	else
	{
		KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
	}
}

NTSTATUS Bus_ProcessReply (PBUSENUM_REPLY repBuffer, PFDO_DEVICE_DATA FdoData)
{
    NTSTATUS            status;
    PLIST_ENTRY         entry;
    PPDO_DEVICE_DATA    pdoData;
    BOOLEAN             found = FALSE;

    ExAcquireFastMutex (&FdoData->Mutex);

	Bus_KdPrint (FdoData, BUS_DBG_SCSI_NOISE,
		("Bus_ProcessReply: SerialNo = %08x\n", repBuffer->handle));

    if (FdoData->NumPDOs == 0)
	{
        //
        // Somebody in user space isn't playing nice!!!
        //
        Bus_KdPrint (FdoData, BUS_DBG_SCSI_ERROR,
                      ("Invalid device id\n"));
        ExReleaseFastMutex (&FdoData->Mutex);
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // Scan the list to find matching PDOs
    //
    for (entry = FdoData->ListOfPDOs.Flink;
         entry != &FdoData->ListOfPDOs;
         entry = entry->Flink) {

        pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);

        if (repBuffer->handle == pdoData->SerialNo)
		{
            found = TRUE;
            break;
        }
    }
    ExReleaseFastMutex (&FdoData->Mutex);

    if (found)
	{
		KIRQL  OldIrql;
		PIRP Irp;
		PIO_STACK_LOCATION irpStack;
		PSCSI_REQUEST_BLOCK Srb;
		PCDB pcdb;

		KeAcquireSpinLock (&pdoData->PDOLock, &OldIrql);
		Irp = pdoData->CurrentIrp;
		pdoData->CurrentIrp = NULL;

		ASSERT (Irp);

		if (Irp == NULL)
		{
			//
			// Somebody in user space isn't playing nice!!!
			//
			Bus_KdPrint (FdoData, BUS_DBG_COMM_ERROR,
				("Bus_ProcessReply: pdoData->CurrentIrp == NULL!!!\n"));
			KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
			return STATUS_SUCCESS;
		}

		IoSetCancelRoutine (Irp, NULL);
		if (Irp->Cancel)
		{
			Irp->IoStatus.Information = 0;
			Irp->IoStatus.Status = STATUS_CANCELLED;
		}
		else
		{
			status = repBuffer->status;
			Irp->IoStatus.Information = repBuffer->numBytes;
			Irp->IoStatus.Status = repBuffer->status;
			irpStack = IoGetCurrentIrpStackLocation (Irp);
			Srb = irpStack->Parameters.Scsi.Srb;
			ASSERT (Srb);
			pcdb = (PCDB)Srb->Cdb;
			Bus_KdPrint (FdoData, BUS_DBG_SCSI_NOISE,
				("Bus_ProcessReply: pdoData = %p, Irp = %p, Srb = %p, DataBuffer = %08x\n",
				pdoData, Irp, Srb, Srb->DataBuffer));
			if (pcdb->CDB6GENERIC.OperationCode == SCSIOP_READ)	// copy data to requestor buffer
			{
				ULONG_PTR offset = (PCHAR)Srb->DataBuffer - (PCHAR)MmGetMdlVirtualAddress(Irp->MdlAddress);
				PVOID buffer = (PCHAR)MmGetSystemAddressForMdlSafe(Irp->MdlAddress,HighPagePriority) + offset;
				RtlCopyMemory (buffer, pdoData->buffer, repBuffer->numBytes);
				Srb->DataTransferLength = repBuffer->numBytes;
			}
			if (status == STATUS_SUCCESS)
			{
				SetSenseInfo (pdoData, Srb->SenseInfoBuffer, SCSI_SENSE_NO_SENSE, SCSI_ADSENSE_NO_SENSE, 0);
				Srb->SrbStatus = SRB_STATUS_SUCCESS;
			}
			else
			{
				SetSenseInfo (pdoData, Srb->SenseInfoBuffer, SCSI_SENSE_HARDWARE_ERROR, SCSI_ADSENSE_LUN_NOT_READY, 0);
				Srb->SrbStatus = SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID;
				Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
			}
		}

		KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		IoReleaseRemoveLock (&pdoData->RemoveLock, Irp);
		BusStartNextPacket (pdoData);
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
}

NTSTATUS
Bus_FDO_IoCtl (
    __in PDEVICE_OBJECT       DeviceObject,
    __in PIRP                 Irp,
    __in PIO_STACK_LOCATION   irpStack,
    __in PFDO_DEVICE_DATA     fdoData
    )
{
    NTSTATUS                status;
    PVOID                   buffer;

    PAGED_CODE ();

	status = IoAcquireRemoveLock (&fdoData->RemoveLock, Irp);
    if (!NT_SUCCESS(status))
	{
        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

    buffer = Irp->AssociatedIrp.SystemBuffer;

    status = STATUS_INVALID_DEVICE_REQUEST;

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_BUSENUM_ADD_DISK:
        if (irpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof (BUSENUM_ADD_DISK))
		{
			status = STATUS_INVALID_PARAMETER;
		}
		else
		{
            Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("AddDisk called\n"));
            status = Bus_AddDisk((PBUSENUM_ADD_DISK)buffer, fdoData);
        }
		Irp->IoStatus.Information = 0;
		Irp->IoStatus.Status = status;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
        break;

    case IOCTL_BUSENUM_REMOVE_DISK:
        if (irpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof (BUSENUM_REMOVE_DISK))
		{
			status = STATUS_INVALID_PARAMETER;
		}
		else
		{
            Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("RemoveDisk called\n"));
            status = Bus_RemoveDisk((PBUSENUM_REMOVE_DISK)buffer, fdoData);
        }
		Irp->IoStatus.Information = 0;
		Irp->IoStatus.Status = status;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
        break;

    case IOCTL_BUSENUM_GET_REQUEST:
        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof (BUSENUM_REQUEST))
		{
			status = STATUS_INVALID_PARAMETER;
			Irp->IoStatus.Information = 0;
			Irp->IoStatus.Status = status;
			IoCompleteRequest (Irp, IO_NO_INCREMENT);
			break;
		}
		// mark irp pending and put it in queue
		Bus_KdPrint(fdoData, BUS_DBG_SCSI_TRACE,
			("Bus_FDO_IoCtl: IOCTL_BUSENUM_GET_REQUEST arrived, Irp = %p\n", Irp));
		IoCsqInsertIrp (&fdoData->RequestQueue.CancelSafeQueue, Irp, NULL);
		// start next operation on all attached PDOs
		{
			PLIST_ENTRY entry;
			for (entry = fdoData->ListOfPDOs.Flink;
				entry != &fdoData->ListOfPDOs;
				entry = entry->Flink)
			{
				PPDO_DEVICE_DATA pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);
				BusStartNextPacket (pdoData);
			}
		}
		status = STATUS_PENDING;
		break;

    case IOCTL_BUSENUM_SEND_REPLY:
        if (irpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof (BUSENUM_REPLY))
		{
			status = STATUS_INVALID_PARAMETER;
		}
		else
		{
			Bus_KdPrint(fdoData, BUS_DBG_SCSI_TRACE, ("Bus_FDO_IoCtl: IOCTL_BUSENUM_SEND_REPLY arrived\n"));
            status = Bus_ProcessReply((PBUSENUM_REPLY)buffer, fdoData);
        }
		Irp->IoStatus.Information = 0;
		Irp->IoStatus.Status = status;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
        break;

    default:
		Irp->IoStatus.Information = 0;
		Irp->IoStatus.Status = status;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
        break; // default status is STATUS_INVALID_PARAMETER
    }

    IoReleaseRemoveLock (&fdoData->RemoveLock, Irp);
    return status;
}

NTSTATUS
Bus_IoCtl (
    __in  PDEVICE_OBJECT  DeviceObject,
    __in  PIRP            Irp
    )
/*++
Routine Description:

    Handle user mode PlugIn, UnPlug and device Eject requests.

Arguments:

   DeviceObject - pointer to a device object.

   Irp - pointer to an I/O Request Packet.

Return Value:

   NT status code

--*/
{
    PIO_STACK_LOCATION      irpStack;
    NTSTATUS                status;
    PCOMMON_DEVICE_DATA     commonData;

    PAGED_CODE ();

    commonData = (PCOMMON_DEVICE_DATA) DeviceObject->DeviceExtension;
    irpStack = IoGetCurrentIrpStackLocation (Irp);

    //
    // If the device has been removed, the driver should
    // not pass the IRP down to the next lower driver.
    //

    if (commonData->DevicePnPState == Deleted) {
        status = STATUS_NO_SUCH_DEVICE;
        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

    if (commonData->IsFDO) {
        //
        // Request is for the bus FDO
        //
        status = Bus_FDO_IoCtl (
                    DeviceObject,
                    Irp,
                    irpStack,
                    (PFDO_DEVICE_DATA) commonData);
    } else {
        //
        // Request is for the child PDO.
        //
        status = Bus_PDO_IoCtl (
                    DeviceObject,
                    Irp,
                    irpStack,
                    (PPDO_DEVICE_DATA) commonData);
    }
	return status;
}


void SetSenseInfo (PPDO_DEVICE_DATA pdoData, PVOID Buffer, UCHAR senseCode, UCHAR asc, UCHAR asq)
{
    PSENSE_DATA psd = (PSENSE_DATA)Buffer;

	pdoData->code = senseCode;
	pdoData->asc = asc;
	pdoData->asq = asq;

	if (psd)
    {
        RtlZeroMemory (psd, sizeof (SENSE_DATA));
        psd->ErrorCode = 0x70;
        psd->Valid = 1;
        psd->SegmentNumber = 0;
        psd->SenseKey = senseCode;
        psd->AdditionalSenseLength = 0;
        psd->AdditionalSenseCode = asc;
        psd->AdditionalSenseCodeQualifier = asq;
    }
}

NTSTATUS BusStartPacket (PPDO_DEVICE_DATA pdoData, PIRP Irp)
{
	NTSTATUS status = STATUS_PENDING;
	KIRQL  OldIrql;

	Bus_KdPrint(pdoData, BUS_DBG_COMM_TRACE, ("BusStartPacket: pdoData = %p, SerialNo = %08x,"
		" Irp = %p\n",
		pdoData, pdoData->SerialNo, Irp));
	KeAcquireSpinLock (&pdoData->PDOLock, &OldIrql);
	Bus_KdPrint(pdoData, BUS_DBG_COMM_TRACE, ("BusStartPacket: pdoData->CurrentIrp = %p\n",
		pdoData->CurrentIrp));
	if (pdoData->CurrentIrp == NULL)
	{
		PDEVICE_OBJECT parentFDO = pdoData->ParentFdo;
	    PFDO_DEVICE_DATA fdoData = (PFDO_DEVICE_DATA) parentFDO->DeviceExtension;
		PIRP rqIrp = IoCsqRemoveNextIrp (&fdoData->RequestQueue.CancelSafeQueue, NULL);
		Bus_KdPrint(pdoData, BUS_DBG_COMM_TRACE, ("BusStartPacket: rqIrp = %p\n",
			rqIrp));
		if (rqIrp == NULL)
		{
			IoCsqInsertIrp (&pdoData->IrpQueue.CancelSafeQueue, Irp, NULL);
			KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
		}
		else
		{
			status = IoAcquireRemoveLock (&pdoData->RemoveLock, Irp);
			if (!NT_SUCCESS(status))
			{
				IoCsqInsertIrp (&fdoData->RequestQueue.CancelSafeQueue, rqIrp, NULL);
				KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
				Irp->IoStatus.Information = 0;
				Irp->IoStatus.Status = status;
				IoCompleteRequest(Irp, IO_NO_INCREMENT);
				return status;
			}
			BusSetupRequest (rqIrp, pdoData, Irp);
			pdoData->CurrentIrp = Irp;
			IoMarkIrpPending (Irp);
			IoSetCancelRoutine(Irp, Bus_CancelIrp);
			KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
			IoCompleteRequest(rqIrp, IO_NO_INCREMENT);
		}
	}
	else
	{
		IoCsqInsertIrp (&pdoData->IrpQueue.CancelSafeQueue, Irp, NULL);
		KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
	}
	return STATUS_PENDING;
}

NTSTATUS Bus_ExecuteCommand (PPDO_DEVICE_DATA pdoData, PSCSI_REQUEST_BLOCK Srb, PIRP Irp)
{
	NTSTATUS status = STATUS_IO_DEVICE_ERROR;
    PCDB pcdb = (PCDB)Srb->Cdb;
	KIRQL  OldIrql;

    switch (pcdb->CDB6GENERIC.OperationCode)
    {
    case SCSIOP_TEST_UNIT_READY:
	case SCSIOP_VERIFY:
        {
			KeAcquireSpinLock (&pdoData->PDOLock, &OldIrql);
			SetSenseInfo (pdoData, Srb->SenseInfoBuffer, SCSI_SENSE_NO_SENSE, SCSI_ADSENSE_NO_SENSE, 0);
			KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			status = STATUS_SUCCESS;
            break;
        }
    case SCSIOP_READ_CAPACITY:
		Bus_KdPrint(pdoData, BUS_DBG_SCSI_TRACE, ("Bus_ExecuteCommand: pdoData = %08x,"
			" Srb = %08x, Srb->SenseInfoBuffer = %08x, command = SCSIOP_READ_CAPACITY\n",
			pdoData, Srb, Srb->SenseInfoBuffer));
		{
			PREAD_CAPACITY_DATA pData = (PREAD_CAPACITY_DATA)Srb->DataBuffer;
			KeAcquireSpinLock (&pdoData->PDOLock, &OldIrql);
			SetSenseInfo (pdoData, Srb->SenseInfoBuffer, 0, 0, 0);
			KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
			RtlZeroMemory (pData, sizeof(READ_CAPACITY_DATA));
			pData->LogicalBlockAddress = pdoData->nBlocks;
			REVERSE_LONG(&pData->LogicalBlockAddress);
			pData->BytesPerBlock = pdoData->BlockSize;
			REVERSE_LONG(&pData->BytesPerBlock);
			SetSenseInfo (pdoData, Srb->SenseInfoBuffer, SCSI_SENSE_NO_SENSE, SCSI_ADSENSE_NO_SENSE, 0);
			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			Irp->IoStatus.Information = Srb->DataTransferLength;
			status = STATUS_SUCCESS;
			break;
		}
    case SCSIOP_READ:
    case SCSIOP_WRITE:
		{
            status = BusStartPacket (pdoData, Irp);
			ASSERT (!NT_SUCCESS(status) || status == STATUS_PENDING);
			break;
		}
    case SCSIOP_MODE_SENSE:
		{
			Bus_KdPrint(pdoData, BUS_DBG_SCSI_TRACE, ("Bus_ExecuteCommand: pdoData = %08x,"
				" Srb = %08x, command = SCSIOP_MODE_SENSE, LogicalUnitNumber = %d, PageCode = %s(%02x), Pc = %d\n",
				pdoData, Srb, pcdb->MODE_SENSE.LogicalUnitNumber,
				ModeSensePageCodeToString(pcdb->MODE_SENSE.PageCode),
				pcdb->MODE_SENSE.PageCode, pcdb->MODE_SENSE.Pc));
			KeAcquireSpinLock (&pdoData->PDOLock, &OldIrql);
			SetSenseInfo (pdoData, Srb->SenseInfoBuffer, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
            Srb->SrbStatus = SRB_STATUS_ERROR;
            Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION | SRB_STATUS_AUTOSENSE_VALID;
			break;
		}
    default:
		{
			Bus_KdPrint(pdoData, BUS_DBG_SCSI_TRACE, ("Bus_ExecuteCommand: pdoData = %08x,"
				" Srb = %08x, Srb->SenseInfoBuffer = %08x, command = %s(%02x)\n",
				pdoData, Srb, Srb->SenseInfoBuffer,
				SCSICommandToString (pcdb->CDB6GENERIC.OperationCode),
				pcdb->CDB6GENERIC.OperationCode));
			KeAcquireSpinLock (&pdoData->PDOLock, &OldIrql);
			SetSenseInfo (pdoData, Srb->SenseInfoBuffer, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
            Srb->SrbStatus = SRB_STATUS_ERROR;
            Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION | SRB_STATUS_AUTOSENSE_VALID;
            break;
        }
    }
	return status;
}

NTSTATUS
Bus_ExecuteSrb (
    __in  PDEVICE_OBJECT  DeviceObject,
    __in  PIRP            Irp,
    __in  PSCSI_REQUEST_BLOCK     srb
    )
/*++

Routine Description:

    Execute SRB for PDO.
    Initialize and start the bus controller. Get the resources
    by parsing the list and map them if required.

Arguments:

    DeviceData - Pointer to the FDO's device extension.
    Irp          - Pointer to the irp.
    srb          - Pointer to the srb.

Return Value:

    NT Status is returned.

--*/
{
    NTSTATUS                status;
    PPDO_DEVICE_DATA        pdoData;
	KIRQL  OldIrql;

    status = STATUS_INVALID_DEVICE_REQUEST;
    pdoData = (PPDO_DEVICE_DATA) DeviceObject->DeviceExtension;

    Bus_KdPrint(pdoData, BUS_DBG_SCSI_NOISE, ("Bus_ExecuteSrb: Srb = %p\n", srb));
	switch (srb->Function)
	{
	case SRB_FUNCTION_CLAIM_DEVICE:
		Bus_KdPrint(pdoData, BUS_DBG_SCSI_TRACE, ("Bus_ExecuteSrb: Function = SRB_FUNCTION_CLAIM_DEVICE\n"));
		KeAcquireSpinLock (&pdoData->PDOLock, &OldIrql);
		if (pdoData->Claimed)
		{
			srb->SrbStatus = SRB_STATUS_BUSY;
			status = STATUS_DEVICE_BUSY;
		}
		else
		{
			pdoData->Claimed = TRUE;
			srb->DataBuffer = DeviceObject;
			srb->SrbStatus = SRB_STATUS_SUCCESS;
			status = STATUS_SUCCESS;
		}
		KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
		break;
	case SRB_FUNCTION_RELEASE_DEVICE:
		Bus_KdPrint(pdoData, BUS_DBG_SCSI_TRACE, ("Bus_ExecuteSrb: Function = SRB_FUNCTION_RELEASE_DEVICE\n"));
		KeAcquireSpinLock (&pdoData->PDOLock, &OldIrql);
		pdoData->Claimed = FALSE;
		srb->SrbStatus = SRB_STATUS_SUCCESS;
		status = STATUS_SUCCESS;
		KeReleaseSpinLock (&pdoData->PDOLock, OldIrql);
		break;
	case SRB_FUNCTION_EXECUTE_SCSI:
		status = Bus_ExecuteCommand (pdoData, srb, Irp);
		break;
	default:
		Bus_KdPrint(pdoData, BUS_DBG_SCSI_TRACE, ("Bus_ExecuteSrb: Function = %02x\n", srb->Function));
		break;
	}
	Bus_KdPrint(pdoData, BUS_DBG_SCSI_NOISE, ("Bus_ExecuteSrb: returning status = %08x\n", status));
	return status;
}

NTSTATUS
Bus_Scsi (
    __in  PDEVICE_OBJECT  DeviceObject,
    __in  PIRP            Irp
    )
/*++
Routine Description:

    Handle IRP_MJ_SCSI requests from FDOs.

Arguments:

   DeviceObject - pointer to a device object.

   Irp - pointer to an I/O Request Packet.

Return Value:

   NT status code

--*/
{
    PIO_STACK_LOCATION      irpStack;
    NTSTATUS                status;
    ULONG                   inlen;
    PPDO_DEVICE_DATA        pdoData;
    PVOID                   buffer;
    PCOMMON_DEVICE_DATA     commonData;
    PSCSI_REQUEST_BLOCK     srb;

    commonData = (PCOMMON_DEVICE_DATA) DeviceObject->DeviceExtension;

    if (commonData->IsFDO) {
        Bus_KdPrint(commonData, BUS_DBG_SCSI_TRACE, ("Bus_Scsi: FDO\n"));
        Irp->IoStatus.Status = status = STATUS_INVALID_DEVICE_REQUEST;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

    pdoData = (PPDO_DEVICE_DATA) DeviceObject->DeviceExtension;

    //
    // Check to see whether the device is removed
    //

	if (pdoData->DevicePnPState == Deleted)
	{
        status = STATUS_NO_SUCH_DEVICE;
        goto END;
    }

    status = IoAcquireRemoveLock (&pdoData->RemoveLock, Irp);
    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);

        return status;
    }


    irpStack = IoGetCurrentIrpStackLocation (Irp);

    buffer = Irp->AssociatedIrp.SystemBuffer;
    srb = irpStack->Parameters.Scsi.Srb;

    status = Bus_ExecuteSrb (DeviceObject, Irp, srb);

END:
	if (status != STATUS_PENDING)
	{
		Irp->IoStatus.Status = status;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
	}
	IoReleaseRemoveLock (&pdoData->RemoveLock, Irp);
    return status;
}

VOID
Bus_DriverUnload (
    __in PDRIVER_OBJECT DriverObject
    )
/*++
Routine Description:
    Clean up everything we did in driver entry.

Arguments:

   DriverObject - pointer to this driverObject.


Return Value:

--*/
{
    PAGED_CODE ();

    Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Unload\n"));

    //
    // All the device objects should be gone.
    //

    ASSERT (NULL == DriverObject->DeviceObject);

    //
    // Here we free all the resources allocated in the DriverEntry
    //

    if (Globals.RegistryPath.Buffer)
        ExFreePool(Globals.RegistryPath.Buffer);

    return;
}


