/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    PNP.C

Abstract:

    This module handles plug & play calls for the vdisk bus controller FDO.

Author:


Environment:

    kernel mode only

Notes:


Revision History:


--*/

#include "busenum.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, Bus_AddDevice)
#pragma alloc_text (PAGE, Bus_PnP)
#pragma alloc_text (PAGE, Bus_AddDisk)
#pragma alloc_text (PAGE, Bus_InitializePdo)
#pragma alloc_text (PAGE, Bus_RemoveFdo)
#pragma alloc_text (PAGE, Bus_FDO_PnP)
#pragma alloc_text (PAGE, Bus_StartFdo)
#pragma alloc_text (PAGE, Bus_SendIrpSynchronously)
#pragma alloc_text (PAGE, Bus_RemoveDisk)
#pragma alloc_text (PAGE, Bus_RemoveAllDisks)
#endif

VOID 
BusInsertIrp (
    IN PIO_CSQ   Csq,
    IN PIRP              Irp
    );

VOID 
BusRemoveIrp(
    IN  PIO_CSQ Csq,
    IN  PIRP    Irp
    );

PIRP 
BusPeekNextIrp(
    IN  PIO_CSQ Csq,
    IN  PIRP    Irp,
    IN  PVOID  PeekContext
    );

VOID 
BusAcquireLock(
    IN  PIO_CSQ Csq,
    OUT PKIRQL  Irql
    );

VOID 
BusReleaseLock(
    IN PIO_CSQ Csq,
    IN KIRQL   Irql
    );

VOID 
BusCompleteCanceledIrp(
    IN  PIO_CSQ             pCsq,
    IN  PIRP                Irp
    );

NTSTATUS InitQueue (PBUS_IO_CSQ csq)
{
	NTSTATUS status = IoCsqInitialize (
		&csq->CancelSafeQueue,
        BusInsertIrp,
        BusRemoveIrp,
        BusPeekNextIrp,
        BusAcquireLock,
        BusReleaseLock,
        BusCompleteCanceledIrp);
    if (!NT_SUCCESS (status))
    {
        return status;
    }
    KeInitializeSpinLock (&csq->QueueLock);
    InitializeListHead (&csq->PendingIrpQueue);
	return STATUS_SUCCESS;
}

NTSTATUS
Bus_AddDevice(
    __in PDRIVER_OBJECT DriverObject,
    __in PDEVICE_OBJECT PhysicalDeviceObject
    )
/*++
Routine Description.

    Our bus has been found.  Attach our FDO to it.
    Allocate any required resources.  Set things up.
    And be prepared for the ``start device''

Arguments:

    DriverObject - pointer to driver object.

    PhysicalDeviceObject  - Device object representing the bus to which we
                            will attach a new FDO.

--*/
{
    NTSTATUS            status;
    PDEVICE_OBJECT      deviceObject = NULL;
    PFDO_DEVICE_DATA    deviceData = NULL;
    PWCHAR              deviceName = NULL;
    ULONG               nameLength;

    PAGED_CODE ();

    Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Add Device: 0x%p\n",
                                          PhysicalDeviceObject));

    status = IoCreateDevice (
                    DriverObject,               // our driver object
                    sizeof (FDO_DEVICE_DATA),   // device object extension size
                    NULL,                       // FDOs do not have names
                    FILE_DEVICE_BUS_EXTENDER,   // We are a bus
                    FILE_DEVICE_SECURE_OPEN,    //
                    TRUE,                       // our FDO is exclusive
                    &deviceObject);             // The device object created

    if (!NT_SUCCESS (status))
    {
        goto End;
    }

    deviceData = (PFDO_DEVICE_DATA) deviceObject->DeviceExtension;
    RtlZeroMemory (deviceData, sizeof (FDO_DEVICE_DATA));

    //
    // Set the initial state of the FDO
    //

    INITIALIZE_PNP_STATE(deviceData);

    deviceData->DebugLevel = BusEnumDebugLevel;

    deviceData->IsFDO = TRUE;

    deviceData->Self = deviceObject;

    ExInitializeFastMutex (&deviceData->Mutex);

    InitializeListHead (&deviceData->ListOfPDOs);

    //
    // Initialize the cancel safe queue
    // 
	status = InitQueue (&deviceData->RequestQueue);

	if (!NT_SUCCESS(status))
	{
        goto End;
	}

	KeInitializeEvent (&deviceData->CleanupReadyEvent, NotificationEvent, FALSE);

    // Set the PDO for use with PlugPlay functions

    deviceData->UnderlyingPDO = PhysicalDeviceObject;

    //
    // Set the initial powerstate of the FDO
    //

    deviceData->DevicePowerState = PowerDeviceUnspecified;
    deviceData->SystemPowerState = PowerSystemWorking;

    IoInitializeRemoveLock (&deviceData->RemoveLock, BUSENUM_POOL_TAG, 0, 0);

    deviceObject->Flags |= DO_POWER_PAGABLE;

    //
    // Tell the Plug & Play system that this device will need a
    // device interface.
    //

    status = IoRegisterDeviceInterface (
                PhysicalDeviceObject,
                (LPGUID) &GUID_DEVINTERFACE_BUSENUM_VDISK,
                NULL,
                &deviceData->InterfaceName);

    if (!NT_SUCCESS (status)) {
        Bus_KdPrint (deviceData, BUS_DBG_SS_ERROR,
                      ("AddDevice: IoRegisterDeviceInterface failed (%x)", status));
        goto End;
    }

    //
    // Attach our FDO to the device stack.
    // The return value of IoAttachDeviceToDeviceStack is the top of the
    // attachment chain.  This is where all the IRPs should be routed.
    //

    deviceData->NextLowerDriver = IoAttachDeviceToDeviceStack (
                                    deviceObject,
                                    PhysicalDeviceObject);

    if (NULL == deviceData->NextLowerDriver) {

        status = STATUS_NO_SUCH_DEVICE;
        goto End;
    }


#if DBG
    //
    // We will demonstrate here the step to retrieve the name of the PDO
    //

    status = IoGetDeviceProperty (PhysicalDeviceObject,
                                  DevicePropertyPhysicalDeviceObjectName,
                                  0,
                                  NULL,
                                  &nameLength);

    if (status != STATUS_BUFFER_TOO_SMALL)
    {
        Bus_KdPrint (deviceData, BUS_DBG_SS_ERROR,
                      ("AddDevice:IoGDP failed (0x%x)\n", status));
        goto End;
    }

    deviceName = ExAllocatePoolWithTag (NonPagedPool,
                            nameLength, BUSENUM_POOL_TAG);

    if (NULL == deviceName) {
        Bus_KdPrint (deviceData, BUS_DBG_SS_ERROR,
        ("AddDevice: no memory to alloc for deviceName(0x%x)\n", nameLength));
        status =  STATUS_INSUFFICIENT_RESOURCES;
        goto End;
    }

    status = IoGetDeviceProperty (PhysicalDeviceObject,
                         DevicePropertyPhysicalDeviceObjectName,
                         nameLength,
                         deviceName,
                         &nameLength);

    if (!NT_SUCCESS (status)) {

        Bus_KdPrint (deviceData, BUS_DBG_SS_ERROR,
                      ("AddDevice:IoGDP(2) failed (0x%x)", status));
        goto End;
    }

    Bus_KdPrint (deviceData, BUS_DBG_SS_TRACE,
                  ("AddDevice: %p to %p->%p (%ws) \n",
                   deviceObject,
                   deviceData->NextLowerDriver,
                   PhysicalDeviceObject,
                   deviceName));

#endif

    //
    // We are done with initializing, so let's indicate that and return.
    // This should be the final step in the AddDevice process.
    //
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

End:
    if (deviceName){
        ExFreePool(deviceName);
    }

    if (!NT_SUCCESS(status) && deviceObject){
        if (deviceData && deviceData->NextLowerDriver){
            IoDetachDevice (deviceData->NextLowerDriver);
        }
        IoDeleteDevice (deviceObject);
    }

    return status;

}

NTSTATUS
Bus_PnP (
    __in PDEVICE_OBJECT   DeviceObject,
    __in PIRP             Irp
    )
/*++
Routine Description:
    Handles PnP Irps sent to both FDO and child PDOs.
Arguments:
    DeviceObject - Pointer to deviceobject
    Irp          - Pointer to a PnP Irp.

Return Value:

    NT Status is returned.
--*/
{
    PIO_STACK_LOCATION      irpStack;
    NTSTATUS                status;
    PCOMMON_DEVICE_DATA     commonData;

    PAGED_CODE ();

    irpStack = IoGetCurrentIrpStackLocation (Irp);
    ASSERT (IRP_MJ_PNP == irpStack->MajorFunction);

    commonData = (PCOMMON_DEVICE_DATA) DeviceObject->DeviceExtension;

    //
    // If the device has been removed, the driver should
    // not pass the IRP down to the next lower driver.
    //

    if (commonData->DevicePnPState == Deleted) {
        Irp->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE ;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }


    if (commonData->IsFDO) {
        Bus_KdPrint (commonData, BUS_DBG_PNP_TRACE,
                      ("FDO %s IRP: 0x%p\n",
                      PnPMinorFunctionString(irpStack->MinorFunction),
                      Irp));
        //
        // Request is for the bus FDO
        //
        status = Bus_FDO_PnP (
                    DeviceObject,
                    Irp,
                    irpStack,
                    (PFDO_DEVICE_DATA) commonData);
    } else {
        Bus_KdPrint (commonData, BUS_DBG_PNP_TRACE,
                      ("PDO %s IRP: 0x%p\n",
                      PnPMinorFunctionString(irpStack->MinorFunction),
                      Irp));
        //
        // Request is for the child PDO.
        //
        status = Bus_PDO_PnP (
                    DeviceObject,
                    Irp,
                    irpStack,
                    (PPDO_DEVICE_DATA) commonData);
    }

    return status;
}

NTSTATUS
Bus_FDO_PnP (
    __in PDEVICE_OBJECT       DeviceObject,
    __in PIRP                 Irp,
    __in PIO_STACK_LOCATION   IrpStack,
    __in PFDO_DEVICE_DATA     DeviceData
    )
/*++
Routine Description:

    Handle requests from the Plug & Play system for the BUS itself

--*/
{
    NTSTATUS            status;
    ULONG               length, prevcount, numPdosPresent;
    PLIST_ENTRY         entry, listHead, nextEntry;
    PPDO_DEVICE_DATA    pdoData;
    PDEVICE_RELATIONS   relations, oldRelations;

    PAGED_CODE ();

    status = IoAcquireRemoveLock (&DeviceData->RemoveLock, Irp);
    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);

        return status;
    }

    switch (IrpStack->MinorFunction) {

    case IRP_MN_START_DEVICE:
        //
        // Send the Irp down and wait for it to come back.
        // Do not touch the hardware until then.
        //
        status = Bus_SendIrpSynchronously (DeviceData->NextLowerDriver, Irp);

        if (NT_SUCCESS(status)) {

            //
            // Initialize your device with the resources provided
            // by the PnP manager to your device.
            //
            status = Bus_StartFdo (DeviceData, Irp);
        }

        //
        // We must now complete the IRP, since we stopped it in the
        // completion routine with MORE_PROCESSING_REQUIRED.
        //

        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock (&DeviceData->RemoveLock, Irp);

        return status;

    case IRP_MN_QUERY_STOP_DEVICE:

        //
        // The PnP manager is trying to stop the device
        // for resource rebalancing. Fail this now if you
        // cannot stop the device in response to STOP_DEVICE.
        //
        SET_NEW_PNP_STATE(DeviceData, StopPending);
        Irp->IoStatus.Status = STATUS_SUCCESS; // You must not fail the IRP.
        break;

    case IRP_MN_CANCEL_STOP_DEVICE:

        //
        // The PnP Manager sends this IRP, at some point after an
        // IRP_MN_QUERY_STOP_DEVICE, to inform the drivers for a
        // device that the device will not be stopped for
        // resource reconfiguration.
        //
        //
        // First check to see whether you have received cancel-stop
        // without first receiving a query-stop. This could happen if
        //  someone above us fails a query-stop and passes down the subsequent
        // cancel-stop.
        //

        if (StopPending == DeviceData->DevicePnPState)
        {
            //
            // We did receive a query-stop, so restore.
            //
            RESTORE_PREVIOUS_PNP_STATE(DeviceData);
            ASSERT(DeviceData->DevicePnPState == Started);
        }
        Irp->IoStatus.Status = STATUS_SUCCESS; // We must not fail the IRP.
        break;

    case IRP_MN_STOP_DEVICE:

        //
        // Free resources given by start device.
        //

        SET_NEW_PNP_STATE(DeviceData, Stopped);

        //
        // We don't need a completion routine so fire and forget.
        //
        // Set the current stack location to the next stack location and
        // call the next device object.
        //

        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_QUERY_REMOVE_DEVICE:

        //
        // If we were to fail this call then we would need to complete the
        // IRP here.  Since we are not, set the status to SUCCESS and
        // call the next driver.
        //

        SET_NEW_PNP_STATE(DeviceData, RemovePending);

        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_CANCEL_REMOVE_DEVICE:

        //
        // If we were to fail this call then we would need to complete the
        // IRP here.  Since we are not, set the status to SUCCESS and
        // call the next driver.
        //

        //
        // First check to see whether you have received cancel-remove
        // without first receiving a query-remove. This could happen if
        // someone above us fails a query-remove and passes down the
        // subsequent cancel-remove.
        //

        if (RemovePending == DeviceData->DevicePnPState)
        {
            //
            // We did receive a query-remove, so restore.
            //
            RESTORE_PREVIOUS_PNP_STATE(DeviceData);
        }
        Irp->IoStatus.Status = STATUS_SUCCESS;// You must not fail the IRP.
        break;

    case IRP_MN_SURPRISE_REMOVAL:

        //
        // The device has been unexpectedly removed from the machine
        // and is no longer available for I/O. Bus_RemoveFdo clears
        // all the resources, frees the interface and de-registers
        // with WMI, but it doesn't delete the FDO. That's done
        // later in Remove device query.
        //

        SET_NEW_PNP_STATE(DeviceData, SurpriseRemovePending);
        Bus_RemoveFdo(DeviceData);

        ExAcquireFastMutex (&DeviceData->Mutex);

        listHead = &DeviceData->ListOfPDOs;

        for(entry = listHead->Flink,nextEntry = entry->Flink;
            entry != listHead;
            entry = nextEntry,nextEntry = entry->Flink) {

            pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);
            RemoveEntryList (&pdoData->Link);
            InitializeListHead (&pdoData->Link);
            pdoData->ParentFdo  = NULL;
            pdoData->ReportedMissing = TRUE;
        }

        ExReleaseFastMutex (&DeviceData->Mutex);

        Irp->IoStatus.Status = STATUS_SUCCESS; // You must not fail the IRP.
        break;

    case IRP_MN_REMOVE_DEVICE:

        //
        // The Plug & Play system has dictated the removal of this device.
        // We have no choice but to detach and delete the device object.
        //

        //
        // Check the state flag to see whether you are surprise removed
        //

        if (DeviceData->DevicePnPState != SurpriseRemovePending)
        {
            Bus_RemoveFdo(DeviceData);
        }

        SET_NEW_PNP_STATE(DeviceData, Deleted);

        //
        // Typically the system removes all the  children before
        // removing the parent FDO. If for any reason child Pdos are
        // still present we will destroy them explicitly, with one exception -
        // we will not delete the PDOs that are in SurpriseRemovePending state.
        //

        ExAcquireFastMutex (&DeviceData->Mutex);

        listHead = &DeviceData->ListOfPDOs;

        for(entry = listHead->Flink,nextEntry = entry->Flink;
            entry != listHead;
            entry = nextEntry,nextEntry = entry->Flink) {

            pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);
            RemoveEntryList (&pdoData->Link);
            if (SurpriseRemovePending == pdoData->DevicePnPState)
            {
                //
                // We will reinitialize the list head so that we
                // wouldn't barf when we try to delink this PDO from
                // the parent's PDOs list, when the system finally
                // removes the PDO. Let's also not forget to set the
                // ReportedMissing flag to cause the deletion of the PDO.
                //
                Bus_KdPrint_Cont(DeviceData, BUS_DBG_PNP_INFO,
                ("\tFound a surprise removed device: 0x%p\n", pdoData->Self));
                InitializeListHead (&pdoData->Link);
                pdoData->ParentFdo  = NULL;
                pdoData->ReportedMissing = TRUE;
                continue;
            }
            DeviceData->NumPDOs--;
            Bus_DestroyPdo (pdoData->Self, pdoData);
        }

        ExReleaseFastMutex (&DeviceData->Mutex);

        IoReleaseRemoveLockAndWait (&DeviceData->RemoveLock, Irp);

        //
        // We need to send the remove down the stack before we detach,
        // but we don't need to wait for the completion of this operation
        // (and to register a completion routine).
        //

        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoSkipCurrentIrpStackLocation (Irp);
        status = IoCallDriver (DeviceData->NextLowerDriver, Irp);
        //
        // Detach from the underlying devices.
        //

        IoDetachDevice (DeviceData->NextLowerDriver);

        Bus_KdPrint_Cont(DeviceData, BUS_DBG_PNP_INFO,
                        ("\tDeleting FDO: 0x%p\n", DeviceObject));

        IoDeleteDevice (DeviceObject);

        return status;

    case IRP_MN_QUERY_DEVICE_RELATIONS:

        Bus_KdPrint_Cont (DeviceData, BUS_DBG_PNP_TRACE,
                   ("\tQueryDeviceRelation Type: %s\n",
                    DbgDeviceRelationString (IrpStack->Parameters.QueryDeviceRelations.Type)));

        if (BusRelations != IrpStack->Parameters.QueryDeviceRelations.Type) {
            //
            // We don't support any other Device Relations
            //
            break;
        }

        //
        // Tell the plug and play system about all the PDOs.
        //
        // There might also be device relations below and above this FDO,
        // so, be sure to propagate the relations from the upper drivers.
        //
        // No Completion routine is needed so long as the status is preset
        // to success.  (PDOs complete plug and play irps with the current
        // IoStatus.Status and IoStatus.Information as the default.)
        //

        ExAcquireFastMutex (&DeviceData->Mutex);

        oldRelations = (PDEVICE_RELATIONS) Irp->IoStatus.Information;
        if (oldRelations) {
            prevcount = oldRelations->Count;
            if (!DeviceData->NumPDOs) {
                //
                // There is a device relations struct already present and we have
                // nothing to add to it, so just call IoSkip and IoCall
                //
                ExReleaseFastMutex (&DeviceData->Mutex);
                break;
            }
        }
        else  {
            prevcount = 0;
        }

        //
        // Calculate the number of PDOs actually present on the bus
        //
        numPdosPresent = 0;
        for (entry = DeviceData->ListOfPDOs.Flink;
             entry != &DeviceData->ListOfPDOs;
             entry = entry->Flink) {
            pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);
            if (pdoData->Present)
                numPdosPresent++;
        }

        //
        // Need to allocate a new relations structure and add our
        // PDOs to it.
        //

        length = sizeof(DEVICE_RELATIONS) +
                ((numPdosPresent + prevcount) * sizeof (PDEVICE_OBJECT)) -1;

        relations = (PDEVICE_RELATIONS) ExAllocatePoolWithTag (PagedPool,
                                        length, BUSENUM_POOL_TAG);

        if (NULL == relations) {
            //
            // Fail the IRP
            //
            ExReleaseFastMutex (&DeviceData->Mutex);
            Irp->IoStatus.Status = status = STATUS_INSUFFICIENT_RESOURCES;
            IoCompleteRequest (Irp, IO_NO_INCREMENT);
            return status;

        }

        //
        // Copy in the device objects so far
        //
        if (prevcount) {
            RtlCopyMemory (relations->Objects, oldRelations->Objects,
                                      prevcount * sizeof (PDEVICE_OBJECT));
        }

        relations->Count = prevcount + numPdosPresent;

        //
        // For each PDO present on this bus add a pointer to the device relations
        // buffer, being sure to take out a reference to that object.
        // The Plug & Play system will dereference the object when it is done
        // with it and free the device relations buffer.
        //

        for (entry = DeviceData->ListOfPDOs.Flink;
             entry != &DeviceData->ListOfPDOs;
             entry = entry->Flink) {

            pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);
            if (pdoData->Present) {
                relations->Objects[prevcount] = pdoData->Self;
                ObReferenceObject (pdoData->Self);
                prevcount++;
            } else {
                pdoData->ReportedMissing = TRUE;
            }
        }

        Bus_KdPrint_Cont (DeviceData, BUS_DBG_PNP_TRACE,
                           ("\t#PDOS present = %d\n\t#PDOs reported = %d\n",
                             DeviceData->NumPDOs, relations->Count));

        //
        // Replace the relations structure in the IRP with the new
        // one.
        //
        if (oldRelations) {
            ExFreePool (oldRelations);
        }
        Irp->IoStatus.Information = (ULONG_PTR) relations;

        ExReleaseFastMutex (&DeviceData->Mutex);

        //
        // Set up and pass the IRP further down the stack
        //
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    default:

        //
        // In the default case we merely call the next driver.
        // We must not modify Irp->IoStatus.Status or complete the IRP.
        //

        break;
    }

    IoSkipCurrentIrpStackLocation (Irp);
    status = IoCallDriver (DeviceData->NextLowerDriver, Irp);
    IoReleaseRemoveLock (&DeviceData->RemoveLock, Irp);
    return status;
}

NTSTATUS
Bus_StartFdo (
    __in  PFDO_DEVICE_DATA            FdoData,
    __in  PIRP   Irp )
/*++

Routine Description:

    Initialize and start the bus controller. Get the resources
    by parsing the list and map them if required.

Arguments:

    DeviceData - Pointer to the FDO's device extension.
    Irp          - Pointer to the irp.

Return Value:

    NT Status is returned.

--*/
{
    NTSTATUS status;
    POWER_STATE powerState;

    PAGED_CODE ();

    //
    // Check the function driver source to learn
    // about parsing resource list.
    //

    //
    // Enable device interface. If the return status is
    // STATUS_OBJECT_NAME_EXISTS means we are enabling the interface
    // that was already enabled, which could happen if the device
    // is stopped and restarted for resource rebalancing.
    //

    status = IoSetDeviceInterfaceState(&FdoData->InterfaceName, TRUE);
    if (!NT_SUCCESS (status)) {
        Bus_KdPrint (FdoData, BUS_DBG_PNP_TRACE,
                ("IoSetDeviceInterfaceState failed: 0x%x\n", status));
        return status;
    }

    //
    // Set the device power state to fully on. Also if this Start
    // is due to resource rebalance, you should restore the device
    // to the state it was before you stopped the device and relinquished
    // resources.
    //

    FdoData->DevicePowerState = PowerDeviceD0;
    powerState.DeviceState = PowerDeviceD0;
    PoSetPowerState ( FdoData->Self, DevicePowerState, powerState );

    SET_NEW_PNP_STATE(FdoData, Started);

    return status;
}

void
Bus_RemoveFdo (
    __in PFDO_DEVICE_DATA FdoData
    )
/*++
Routine Description:

    Frees any memory allocated by the FDO and unmap any IO mapped as well.
    This function does not the delete the deviceobject.

Arguments:

    DeviceData - Pointer to the FDO's device extension.

Return Value:

    NT Status is returned.

--*/
{
    PAGED_CODE ();

    //
    // Stop all access to the device, fail any outstanding I/O to the device,
    // and free all the resources associated with the device.
    //

    //
    // Disable the device interface and free the buffer
    //

    if (FdoData->InterfaceName.Buffer != NULL) {

        IoSetDeviceInterfaceState (&FdoData->InterfaceName, FALSE);

        ExFreePool (FdoData->InterfaceName.Buffer);
        RtlZeroMemory (&FdoData->InterfaceName,
                       sizeof (UNICODE_STRING));
    }

}

NTSTATUS
Bus_SendIrpSynchronously (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    )
/*++
Routine Description:

    Sends the Irp down to lower driver and waits for it to
    come back by setting a completion routine.

Arguments:
    DeviceObject - Pointer to deviceobject
    Irp          - Pointer to a PnP Irp.

Return Value:

    NT Status is returned.

--*/
{
    KEVENT   event;
    NTSTATUS status;

    PAGED_CODE();

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    IoCopyCurrentIrpStackLocationToNext(Irp);

    IoSetCompletionRoutine(Irp,
                           Bus_CompletionRoutine,
                           &event,
                           TRUE,
                           TRUE,
                           TRUE
                           );

    status = IoCallDriver(DeviceObject, Irp);

    //
    // Wait for lower drivers to be done with the Irp.
    // Important thing to note here is when you allocate
    // the memory for an event in the stack you must do a
    // KernelMode wait instead of UserMode to prevent
    // the stack from getting paged out.
    //

    if (status == STATUS_PENDING) {
       KeWaitForSingleObject(&event,
                             Executive,
                             KernelMode,
                             FALSE,
                             NULL
                             );
       status = Irp->IoStatus.Status;
    }

    return status;
}

NTSTATUS
Bus_CompletionRoutine(
    __in PDEVICE_OBJECT   DeviceObject,
    __in PIRP             Irp,
    __in PVOID            Context
    )
/*++
Routine Description:
    A completion routine for use when calling the lower device objects to
    which our bus (FDO) is attached.

Arguments:

    DeviceObject - Pointer to deviceobject
    Irp          - Pointer to a PnP Irp.
    Context      - Pointer to an event object
Return Value:

    NT Status is returned.

--*/
{
    UNREFERENCED_PARAMETER (DeviceObject);

    //
    // If the lower driver didn't return STATUS_PENDING, we don't need to
    // set the event because we won't be waiting on it.
    // This optimization avoids grabbing the dispatcher lock and improves perf.
    //
    if (Irp->PendingReturned == TRUE) {

        KeSetEvent ((PKEVENT) Context, IO_NO_INCREMENT, FALSE);
    }
    return STATUS_MORE_PROCESSING_REQUIRED; // Keep this IRP
}

VOID UnmapBuffer (PPDO_DEVICE_DATA pdoData)
{
	MmUnmapLockedPages (pdoData->buffer, pdoData->bufferMdl);
	MmUnlockPages (pdoData->bufferMdl);
	IoFreeMdl (pdoData->bufferMdl);
}

NTSTATUS MapBuffer (PPDO_DEVICE_DATA pdoData, PBUSENUM_ADD_DISK AddDisk)
{
	pdoData->bufferMdl = IoAllocateMdl (AddDisk->buffer, BUFFER_SIZE, FALSE, FALSE, NULL);
	if (pdoData->bufferMdl == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	__try
	{
		ProbeForWrite (AddDisk->buffer, BUFFER_SIZE, BUFFER_ALIGNMENT);
		MmProbeAndLockPages (pdoData->bufferMdl, KernelMode, IoWriteAccess);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		IoFreeMdl (pdoData->bufferMdl);
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	pdoData->buffer = MmMapLockedPagesSpecifyCache (pdoData->bufferMdl, KernelMode, MmCached, NULL, FALSE, LowPagePriority);
	if (pdoData->buffer == NULL)
	{
		MmUnlockPages (pdoData->bufferMdl);
		IoFreeMdl (pdoData->bufferMdl);
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	return STATUS_SUCCESS;
}

NTSTATUS
Bus_DestroyPdo (
    PDEVICE_OBJECT      Device,
    PPDO_DEVICE_DATA    PdoData
    )
/*++
Routine Description:
    The Plug & Play subsystem has instructed that this PDO should be removed.

    We should therefore
    - Complete any requests queued in the driver
    - If the device is still attached to the system,
    - then complete the request and return.
    - Otherwise, cleanup device specific allocations, memory, events...
    - Call IoDeleteDevice
    - Return from the dispatch routine.

    Note that if the device is still connected to the bus (IE in this case
    the enum application has not yet told us that the device has disappeared)
    then the PDO must remain around, and must be returned during any
    query Device relations IRPS.

--*/

{
	KIRQL  OldIrql;
	PIRP pendingIrp;

	// cancelling all IRPs and disabling new ones processing

	KeAcquireSpinLock (&PdoData->PDOLock, &OldIrql);
	if (PdoData->CurrentIrp)
	{
		pendingIrp = PdoData->CurrentIrp;
		PdoData->CurrentIrp = NULL;
		IoSetCancelRoutine (pendingIrp, NULL);
		pendingIrp->IoStatus.Information = 0;
		pendingIrp->IoStatus.Status = STATUS_CANCELLED;
		KeReleaseSpinLock (&PdoData->PDOLock, OldIrql);
		IoCompleteRequest(pendingIrp, IO_NO_INCREMENT);
		IoReleaseRemoveLock (&PdoData->RemoveLock, pendingIrp);
	}
	else
	{
		KeReleaseSpinLock (&PdoData->PDOLock, OldIrql);
	}

	while (pendingIrp = IoCsqRemoveNextIrp(&PdoData->IrpQueue.CancelSafeQueue, NULL))
	{

		//
		// Cancel the IRP
		//

		Bus_KdPrint (PdoData, BUS_DBG_IOCTL_TRACE, ("ProcessRemoveDevice: pendingIrp = %p\n", pendingIrp));
		IoSetCancelRoutine (pendingIrp, NULL);
		pendingIrp->IoStatus.Information = 0;
		pendingIrp->IoStatus.Status = STATUS_CANCELLED;
		IoCompleteRequest(pendingIrp, IO_NO_INCREMENT);
	}

	//
    // Free any resources.
    //

    if (PdoData->HardwareIDs) {
        ExFreePool (PdoData->HardwareIDs);
        PdoData->HardwareIDs = NULL;
    }

    Bus_KdPrint_Cont(PdoData, BUS_DBG_PNP_INFO,
                        ("\tDeleting PDO: 0x%p\n", Device));
	UnmapBuffer (PdoData);
	KeSetEvent (PdoData->RemoveEvent, 0, FALSE);
    IoDeleteDevice (Device);
    return STATUS_SUCCESS;
}

NTSTATUS
Bus_InitializePdo (
    __drv_in(__drv_aliasesMem)
    PDEVICE_OBJECT      Pdo,
    PFDO_DEVICE_DATA    FdoData,
	PBUSENUM_ADD_DISK   AddDisk
    )
/*++
Routine Description:
    Set the PDO into a known good starting state

--*/
{
	NTSTATUS status;
    PPDO_DEVICE_DATA pdoData;

    PAGED_CODE ();

    pdoData = (PPDO_DEVICE_DATA)  Pdo->DeviceExtension;

    Bus_KdPrint(pdoData, BUS_DBG_PNP_NOISE,
                 ("pdo 0x%p, extension 0x%p\n", Pdo, pdoData));

    //
    // Initialize the rest
    //
    pdoData->IsFDO = FALSE;
    pdoData->Self =  Pdo;
    pdoData->DebugLevel = BusEnumDebugLevel;

    pdoData->ParentFdo = FdoData->Self;

    pdoData->Present = TRUE; // attached to the bus
    pdoData->ReportedMissing = FALSE; // not yet reported missing
	pdoData->Claimed = FALSE;	// not claimed by class drivers
	pdoData->nBlocks = AddDisk->nBlocks;
	pdoData->BlockSize = AddDisk->BlockSize;
	KeInitializeSpinLock (&pdoData->PDOLock);
	pdoData->CurrentIrp = NULL;

	status = ObReferenceObjectByHandle (AddDisk->RemoveEvent,
		STANDARD_RIGHTS_ALL, NULL, KernelMode, &pdoData->RemoveEvent, NULL);
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice (Pdo);
		return status;
	}

	//
    // Initialize the cancel safe queue
    // 
	status = InitQueue (&pdoData->IrpQueue);

	status = MapBuffer (pdoData, AddDisk);
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice (Pdo);
		return status;
	}

    INITIALIZE_PNP_STATE(pdoData);

    //
    // PDO's usually start their life at D3
    //

    pdoData->DevicePowerState = PowerDeviceD3;
    pdoData->SystemPowerState = PowerSystemWorking;

    IoInitializeRemoveLock (&pdoData->RemoveLock, BUSENUM_POOL_TAG, 0, 0);
    Pdo->Flags |= DO_POWER_PAGABLE;

    ExAcquireFastMutex (&FdoData->Mutex);
    InsertTailList(&FdoData->ListOfPDOs, &pdoData->Link);
    FdoData->NumPDOs++;
    ExReleaseFastMutex (&FdoData->Mutex);

    // This should be the last step in initialization.
    Pdo->Flags &= ~DO_DEVICE_INITIALIZING;
	return STATUS_SUCCESS;
}

NTSTATUS
Bus_AddDisk (
    PBUSENUM_ADD_DISK   AddDisk,
    PFDO_DEVICE_DATA    FdoData
    )
/*++
Routine Description:
    The user application has told us that a new device on the bus has arrived.

    We therefore need to create a new PDO, initialize it, add it to the list
    of PDOs for this FDO bus, and then tell Plug and Play that all of this
    happened so that it will start sending prodding IRPs.
--*/
{
    PDEVICE_OBJECT      pdo;
    PPDO_DEVICE_DATA    pdoData;
    NTSTATUS            status;
    ULONG               length;
    BOOLEAN             unique;
    PLIST_ENTRY         entry;

    PAGED_CODE ();


    //
    // Following code walks the PDO list to check whether
    // the input serial number is unique.
    // Let's first assume it's unique
    //
    unique = TRUE;

    ExAcquireFastMutex (&FdoData->Mutex);

    for (entry = FdoData->ListOfPDOs.Flink;
         entry != &FdoData->ListOfPDOs;
         entry = entry->Flink) {

        pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);
        if (AddDisk->handle == pdoData->SerialNo &&
            pdoData->DevicePnPState != SurpriseRemovePending)
        {
            unique = FALSE;
            break;
        }
    }

    ExReleaseFastMutex (&FdoData->Mutex);

    //
    // Just be aware: If a user unplugs and plugs in the device
    // immediately, in a split second, even before we had a chance to
    // clean up the previous PDO completely, we might fail to
    // accept the device.
    //

    if (!unique)
        return STATUS_INVALID_PARAMETER;

    //
    // Create the PDO
    //

    Bus_KdPrint(FdoData, BUS_DBG_PNP_NOISE,
                 ("FdoData->NextLowerDriver = 0x%p\n", FdoData->NextLowerDriver));

    //
    // PDO must have a name. You should let the system auto generate a
    // name by specifying FILE_AUTOGENERATED_DEVICE_NAME in the
    // DeviceCharacteristics parameter. Let us create a secure deviceobject,
    // in case the child gets installed as a raw device (RawDeviceOK), to prevent
    // an unpriviledged user accessing our device. This function is avaliable
    // in a static WDMSEC.LIB and can be used in Win2k, XP, and Server 2003
    // Just make sure that  the GUID specified here is not a setup class GUID.
    // If you specify a setup class guid, you must make sure that class is
    // installed before enumerating the PDO.
    //

    status = IoCreateDevice(FdoData->Self->DriverObject,
                sizeof (PDO_DEVICE_DATA),
                NULL,
                FILE_DEVICE_MASS_STORAGE,
                FILE_AUTOGENERATED_DEVICE_NAME |FILE_DEVICE_SECURE_OPEN,
                FALSE,
                &pdo);
    if (!NT_SUCCESS (status))
	{
        return status;
    }

    pdoData = (PPDO_DEVICE_DATA) pdo->DeviceExtension;

    //
    // Copy the hardware IDs
    //

    pdoData->HardwareIDs =
            ExAllocatePoolWithTag (NonPagedPool, BUS_HARDWARE_IDS_LENGTH, BUSENUM_POOL_TAG);

    if (NULL == pdoData->HardwareIDs) {
        IoDeleteDevice(pdo);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory (pdoData->HardwareIDs, BUS_HARDWARE_IDS, BUS_HARDWARE_IDS_LENGTH);
    pdoData->SerialNo = AddDisk->handle;
    status = Bus_InitializePdo (pdo, FdoData, AddDisk);
    if (!NT_SUCCESS (status))
	{
        return status;
    }

    //
    // Device Relation changes if a new pdo is created. So let
    // the PNP system now about that. This forces it to send bunch of pnp
    // queries and cause the function driver to be loaded.
    //

    IoInvalidateDeviceRelations (FdoData->UnderlyingPDO, BusRelations);

    return status;
}

NTSTATUS Bus_RemoveAllDisks (
	PFDO_DEVICE_DATA     FdoData
	)
{
    PLIST_ENTRY         entry;
    PPDO_DEVICE_DATA    pdoData;

    PAGED_CODE ();

    ExAcquireFastMutex (&FdoData->Mutex);

    if (FdoData->NumPDOs == 0)
	{
        ExReleaseFastMutex (&FdoData->Mutex);
        return STATUS_SUCCESS;
    }

    //
    // Scan the list and eject all PDOs
    //
    for (entry = FdoData->ListOfPDOs.Flink;
         entry != &FdoData->ListOfPDOs;
         entry = entry->Flink)
	{
		pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);
		pdoData->Present = FALSE;
		IoInvalidateDeviceRelations (FdoData->UnderlyingPDO, BusRelations);
    }
    ExReleaseFastMutex (&FdoData->Mutex);
    return STATUS_SUCCESS;
}

NTSTATUS
Bus_RemoveDisk (
    PBUSENUM_REMOVE_DISK RemoveDisk,
    PFDO_DEVICE_DATA     FdoData
    )
{
    PLIST_ENTRY         entry;
    PPDO_DEVICE_DATA    pdoData;
    BOOLEAN             found = FALSE;

    PAGED_CODE ();

    ExAcquireFastMutex (&FdoData->Mutex);

	Bus_KdPrint (FdoData, BUS_DBG_IOCTL_NOISE,
		("Bus_RemoveDisk: SerialNo = Ejecting %d\n", RemoveDisk->handle));

    if (FdoData->NumPDOs == 0) {
        //
        // Somebody in user space isn't playing nice!!!
        //
        Bus_KdPrint (FdoData, BUS_DBG_IOCTL_ERROR,
                      ("No devices to eject!\n"));
        ExReleaseFastMutex (&FdoData->Mutex);
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // Scan the list to find matching PDOs
    //
    for (entry = FdoData->ListOfPDOs.Flink;
         entry != &FdoData->ListOfPDOs;
         entry = entry->Flink)
	{
        pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);
        if (RemoveDisk->handle == pdoData->SerialNo)
		{
            found = TRUE;
			KeResetEvent (pdoData->RemoveEvent);
			IoRequestDeviceEject(pdoData->Self);
			break;
        }
    }
    ExReleaseFastMutex (&FdoData->Mutex);

    if (found)
	{
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
}

VOID 
BusInsertIrp (
    IN PIO_CSQ   Csq,
    IN PIRP              Irp
    )
{
    PBUS_IO_CSQ bcsq = CONTAINING_RECORD(Csq, 
                                 BUS_IO_CSQ, CancelSafeQueue);
    InsertTailList(&bcsq->PendingIrpQueue, 
                         &Irp->Tail.Overlay.ListEntry);
}

VOID 
BusRemoveIrp(
    IN  PIO_CSQ Csq,
    IN  PIRP    Irp
    )
{
    PBUS_IO_CSQ bcsq = CONTAINING_RECORD(Csq, 
                                 BUS_IO_CSQ, CancelSafeQueue);
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
}

PIRP 
BusPeekNextIrp(
    IN  PIO_CSQ Csq,
    IN  PIRP    Irp,
    IN  PVOID  PeekContext
    )
{
	PBUS_IO_CSQ bcsq;
    PIRP                    nextIrp = NULL;
    PLIST_ENTRY             nextEntry;
    PLIST_ENTRY             listHead;
    PIO_STACK_LOCATION     irpStack;

    bcsq = CONTAINING_RECORD(Csq, 
                                 BUS_IO_CSQ, CancelSafeQueue);
    
    listHead = &bcsq->PendingIrpQueue;

    // 
    // If the IRP is NULL, we will start peeking from the listhead, else
    // we will start from that IRP onwards. This is done under the
    // assumption that new IRPs are always inserted at the tail.
    //
        
    if(Irp == NULL) {       
        nextEntry = listHead->Flink;
    } else {
        nextEntry = Irp->Tail.Overlay.ListEntry.Flink;
    }
    
    while(nextEntry != listHead) {
        
        nextIrp = CONTAINING_RECORD(nextEntry, IRP, Tail.Overlay.ListEntry);

        irpStack = IoGetCurrentIrpStackLocation(nextIrp);
        
        //
        // If context is present, continue until you find a matching one.
        // Else you break out as you got next one.
        //
        
        if(PeekContext) {
            if(irpStack->FileObject == (PFILE_OBJECT) PeekContext) {       
                break;
            }
        } else {
            break;
        }
        nextIrp = NULL;
        nextEntry = nextEntry->Flink;
    }

    return nextIrp;
    
}

__drv_raisesIRQL(DISPATCH_LEVEL)
__drv_maxIRQL(DISPATCH_LEVEL)
VOID 
BusAcquireLock(
    IN  PIO_CSQ Csq,
    OUT __drv_out_deref(__drv_savesIRQL) PKIRQL  Irql
    )
{
    PBUS_IO_CSQ bcsq = CONTAINING_RECORD(Csq, 
                                 BUS_IO_CSQ, CancelSafeQueue);
    KeAcquireSpinLock(&bcsq->QueueLock, Irql);
}

__drv_requiresIRQL(DISPATCH_LEVEL)
VOID 
BusReleaseLock(
    IN PIO_CSQ Csq,
    IN __drv_in(__drv_restoresIRQL)   KIRQL   Irql
    )
{
    PBUS_IO_CSQ bcsq = CONTAINING_RECORD(Csq, 
                                 BUS_IO_CSQ, CancelSafeQueue);
    KeReleaseSpinLock(&bcsq->QueueLock, Irql);
}

VOID 
BusCompleteCanceledIrp(
    IN  PIO_CSQ             Csq,
    IN  PIRP                Irp
    )
{
    PBUS_IO_CSQ bcsq = CONTAINING_RECORD(Csq, 
                                 BUS_IO_CSQ, CancelSafeQueue);
	DbgPrint ("BusCompleteCanceledIrp: Irp = %p\n", Irp);
    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}

#if DBG

PCHAR
PnPMinorFunctionString (
    UCHAR MinorFunction
)
{
    switch (MinorFunction)
    {
        case IRP_MN_START_DEVICE:
            return "IRP_MN_START_DEVICE";
        case IRP_MN_QUERY_REMOVE_DEVICE:
            return "IRP_MN_QUERY_REMOVE_DEVICE";
        case IRP_MN_REMOVE_DEVICE:
            return "IRP_MN_REMOVE_DEVICE";
        case IRP_MN_CANCEL_REMOVE_DEVICE:
            return "IRP_MN_CANCEL_REMOVE_DEVICE";
        case IRP_MN_STOP_DEVICE:
            return "IRP_MN_STOP_DEVICE";
        case IRP_MN_QUERY_STOP_DEVICE:
            return "IRP_MN_QUERY_STOP_DEVICE";
        case IRP_MN_CANCEL_STOP_DEVICE:
            return "IRP_MN_CANCEL_STOP_DEVICE";
        case IRP_MN_QUERY_DEVICE_RELATIONS:
            return "IRP_MN_QUERY_DEVICE_RELATIONS";
        case IRP_MN_QUERY_INTERFACE:
            return "IRP_MN_QUERY_INTERFACE";
        case IRP_MN_QUERY_CAPABILITIES:
            return "IRP_MN_QUERY_CAPABILITIES";
        case IRP_MN_QUERY_RESOURCES:
            return "IRP_MN_QUERY_RESOURCES";
        case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
            return "IRP_MN_QUERY_RESOURCE_REQUIREMENTS";
        case IRP_MN_QUERY_DEVICE_TEXT:
            return "IRP_MN_QUERY_DEVICE_TEXT";
        case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:
            return "IRP_MN_FILTER_RESOURCE_REQUIREMENTS";
        case IRP_MN_READ_CONFIG:
            return "IRP_MN_READ_CONFIG";
        case IRP_MN_WRITE_CONFIG:
            return "IRP_MN_WRITE_CONFIG";
        case IRP_MN_EJECT:
            return "IRP_MN_EJECT";
        case IRP_MN_SET_LOCK:
            return "IRP_MN_SET_LOCK";
        case IRP_MN_QUERY_ID:
            return "IRP_MN_QUERY_ID";
        case IRP_MN_QUERY_PNP_DEVICE_STATE:
            return "IRP_MN_QUERY_PNP_DEVICE_STATE";
        case IRP_MN_QUERY_BUS_INFORMATION:
            return "IRP_MN_QUERY_BUS_INFORMATION";
        case IRP_MN_DEVICE_USAGE_NOTIFICATION:
            return "IRP_MN_DEVICE_USAGE_NOTIFICATION";
        case IRP_MN_SURPRISE_REMOVAL:
            return "IRP_MN_SURPRISE_REMOVAL";
        case IRP_MN_QUERY_LEGACY_BUS_INFORMATION:
            return "IRP_MN_QUERY_LEGACY_BUS_INFORMATION";
        default:
            return "unknown_pnp_irp";
    }
}

PCHAR
DbgDeviceRelationString(
    __in DEVICE_RELATION_TYPE Type
    )
{
	static char s[40];
    switch (Type)
    {
        case BusRelations:
            return "BusRelations";
        case EjectionRelations:
            return "EjectionRelations";
        case RemovalRelations:
            return "RemovalRelations";
        case TargetDeviceRelation:
            return "TargetDeviceRelation";
        default:
			RtlStringCbPrintfA (s, sizeof s, "UnKnown Relation: %d", Type);
            return s;
    }
}

PCHAR
DbgDeviceIDString(
    BUS_QUERY_ID_TYPE Type
    )
{
    switch (Type)
    {
        case BusQueryDeviceID:
            return "BusQueryDeviceID";
        case BusQueryHardwareIDs:
            return "BusQueryHardwareIDs";
        case BusQueryCompatibleIDs:
            return "BusQueryCompatibleIDs";
        case BusQueryInstanceID:
            return "BusQueryInstanceID";
        case BusQueryDeviceSerialNumber:
            return "BusQueryDeviceSerialNumber";
        default:
            return "UnKnown ID";
    }
}

#endif


