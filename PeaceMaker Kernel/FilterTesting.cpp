/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 * 
 * COPYRIGHT Bill Demirkapi 2020
 */
#include "common.h"
#include "IOCTLCommunication.h"

PIOCTL_COMMUNICATION Communicator;

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

/*************************************************************************
    Prototypes
*************************************************************************/

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    );

NTSTATUS
FilterUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    );

EXTERN_C_END

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, FilterUnload)
#endif

/*************************************************************************
    MiniFilter initialization and unload routines.
*************************************************************************/

/**
	Initialize the mini-filter driver.
	DriverObject - The driver's object.
	RegistryPath - The path to the driver's registry entry.
*/
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS status;

	status = STATUS_SUCCESS;

	DBGPRINT("FilterTesting!DriverEntry: Hello world.");

	Communicator = new (NonPagedPool, 'cImP') IOCTLCommunication(DriverObject, RegistryPath, FilterUnload, &status);
	if (NT_SUCCESS(status) == FALSE)
	{
		DBGPRINT("FilterTesting!DriverEntry: Failed to initialize communication with status 0x%X.", status);
	}

    return status;
}

/**
	This function handles unloading the mini-filter.
	@param Flags - Flags indicating whether or not this is a mandatory unload.
*/
NTSTATUS
FilterUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

	DBGPRINT("FilterTesting!FilterUnload: Unloading filter.");

	Communicator->~IOCTLCommunication();
	ExFreePoolWithTag(Communicator, 'cImP');

    return STATUS_SUCCESS;
}