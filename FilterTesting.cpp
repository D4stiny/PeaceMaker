#include "common.h"
#include "FSFilter.h"
#include "RegistryFilter.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

PFLT_FILTER gFilterHandle;
PFS_BLOCKING_FILTER FilesystemMonitor;
PREGISTRY_BLOCKING_FILTER RegistryMonitor;

#define FILE_MONITOR_TAG 'mFmP'
#define REGISTRY_MONITOR_TAG 'mRmP'

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

    UNREFERENCED_PARAMETER( RegistryPath );

	status = STATUS_SUCCESS;

	DBGPRINT("FilterTesting!DriverEntry: Hello world.");
	FilesystemMonitor = new (PagedPool, FILE_MONITOR_TAG) FSBlockingFilter(DriverObject, FilterUnload, &status, &gFilterHandle);
	if (NT_SUCCESS(status) == FALSE)
	{
		DBGPRINT("FilterTesting!DriverEntry: Failed to initialize the filesystem blocking filter with status 0x%X.", status);
		goto Exit;
	}

	FilesystemMonitor->GetStringFilters()->AddFilter(L"preventdelete", FILTER_FLAG_DELETE);
	FilesystemMonitor->GetStringFilters()->AddFilter(L"preventexecute", FILTER_FLAG_EXECUTE);


	ULONG test = FilesystemMonitor->GetStringFilters()->AddFilter(L"preventnothing", FILTER_FLAG_WRITE);
	if (FilesystemMonitor->GetStringFilters()->RemoveFilter(test))
	{
		DBGPRINT("FilterTesting!DriverEntry: Removed filter successfully.");
	}

	FilesystemMonitor->GetStringFilters()->AddFilter(L"preventwrite", FILTER_FLAG_WRITE);
	FilesystemMonitor->GetStringFilters()->AddFilter(L"preventall", FILTER_FLAG_ALL);

	RegistryMonitor = new (PagedPool, REGISTRY_MONITOR_TAG) RegistryBlockingFilter(DriverObject, &status);
	if (NT_SUCCESS(status) == FALSE)
	{
		DBGPRINT("FilterTesting!DriverEntry: Failed to initialize the registry blocking filter with status 0x%X.", status);
		goto Exit;
	}

	RegistryMonitor->GetStringFilters()->AddFilter(L"preventwrite", FILTER_FLAG_WRITE);
	RegistryMonitor->GetStringFilters()->AddFilter(L"preventdelete", FILTER_FLAG_DELETE);
	RegistryMonitor->GetStringFilters()->AddFilter(L"preventall", FILTER_FLAG_ALL);


Exit:
    return status;
}

/**
	This function handles unloading the mini-filter.
	Flags - Flags indicating whether or not this is a mandatory unload.
*/
NTSTATUS
FilterUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

	DBGPRINT("FilterTesting!FilterUnload: Unloading filter.");

    FltUnregisterFilter( gFilterHandle );

	//
	// Make sure to deconstruct the classes.
	//
	FilesystemMonitor->~FSBlockingFilter();
	ExFreePoolWithTag(FilesystemMonitor, FILE_MONITOR_TAG);
	RegistryMonitor->~RegistryBlockingFilter();
	ExFreePoolWithTag(RegistryMonitor, REGISTRY_MONITOR_TAG);

    return STATUS_SUCCESS;
}