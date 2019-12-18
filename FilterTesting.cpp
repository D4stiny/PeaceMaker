#include "common.h"
#include "Filters.h"

#define FlagOn(_F,_SF) ((_F) & (_SF))
#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

PFLT_FILTER gFilterHandle;
PCUSTOM_FILTERS fileFilters;

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
FilterTestingUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
FilterTestingPreCreateOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

FLT_PREOP_CALLBACK_STATUS
FilterTestingPreWriteOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

FLT_PREOP_CALLBACK_STATUS
FilterTestingPreSetInfoOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

NTSTATUS
FilterTestingInstanceSetup(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_SETUP_FLAGS Flags,
	_In_ DEVICE_TYPE VolumeDeviceType,
	_In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
);

NTSTATUS
FilterTestingInstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
);

VOID
FilterTestingInstanceTeardownStart(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
);

VOID
FilterTestingInstanceTeardownComplete(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
);

EXTERN_C_END

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, FilterTestingInstanceSetup)
#pragma alloc_text(PAGE, FilterTestingInstanceQueryTeardown)
#pragma alloc_text(PAGE, FilterTestingInstanceTeardownStart)
#pragma alloc_text(PAGE, FilterTestingInstanceTeardownComplete)
#pragma alloc_text(PAGE, FilterTestingUnload)
#endif

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {

    { IRP_MJ_CREATE,
      0,
      FilterTestingPreCreateOperation,
      NULL },

	{ IRP_MJ_WRITE,
	  0,
	  FilterTestingPreWriteOperation,
	  NULL },

    { IRP_MJ_SET_INFORMATION,
	  FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,
      FilterTestingPreSetInfoOperation,
	  NULL },

    { IRP_MJ_OPERATION_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof( FLT_REGISTRATION ),         //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    NULL,                               //  Context
    Callbacks,                          //  Operation callbacks

    FilterTestingUnload,                           //  MiniFilterUnload

	FilterTestingInstanceSetup,                    //  InstanceSetup
	FilterTestingInstanceQueryTeardown,            //  InstanceQueryTeardown
	FilterTestingInstanceTeardownStart,            //  InstanceTeardownStart
	FilterTestingInstanceTeardownComplete,         //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};


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

	DBGPRINT("FilterTesting!DriverEntry: Entered");

	fileFilters = new (PagedPool) CUSTOM_FILTERS();
	fileFilters->AddFilter(L"preventdeletion");

    //
    //  Register with FltMgr to tell it our callback routines
    //

    status = FltRegisterFilter( DriverObject,
                                &FilterRegistration,
                                &gFilterHandle );

    FLT_ASSERT( NT_SUCCESS( status ) );

    if (NT_SUCCESS( status )) {

        //
        //  Start filtering i/o
        //

        status = FltStartFiltering( gFilterHandle );

        if (!NT_SUCCESS( status )) {

            FltUnregisterFilter( gFilterHandle );
        }
    }

    return status;
}

/**
	This function handles unloading the mini-filter.
	Flags - Flags indicating whether or not this is a mandatory unload.
*/
NTSTATUS
FilterTestingUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

	DBGPRINT("FilterTesting!FilterTestingUnload: Unloading filter.");

    FltUnregisterFilter( gFilterHandle );

	fileFilters->DestructCustomFilters();
	delete fileFilters;

    return STATUS_SUCCESS;
}


/*************************************************************************
    MiniFilter callback routines.
*************************************************************************/

/**
	This function determines whether or not the mini-filter should attach to the volume.
	FltObjects - Objects related to the filter, instance, and its associated volume.
	Flags - Flags that indicate the reason for the volume attach request.
	VolumeDeviceType - The device type of the specified volume.
	VolumeFilesystemType - The filesystem type of the specified volume.
*/
NTSTATUS
FilterTestingInstanceSetup (
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_SETUP_FLAGS Flags,
	_In_ DEVICE_TYPE VolumeDeviceType,
	_In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	BOOLEAN isWritable = FALSE;

	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(VolumeDeviceType);

	PAGED_CODE();

	status = FltIsVolumeWritable(FltObjects->Volume,
		&isWritable);

	if (!NT_SUCCESS(status)) {

		return STATUS_FLT_DO_NOT_ATTACH;
	}

	//
	// If you can't write to a volume... how can you delete a file in it?
	//
	if (isWritable) {

		switch (VolumeFilesystemType) {

		case FLT_FSTYPE_NTFS:
		case FLT_FSTYPE_REFS:

			status = STATUS_SUCCESS;
			break;

		default:

			return STATUS_FLT_DO_NOT_ATTACH;
		}

	}
	else {

		return STATUS_FLT_DO_NOT_ATTACH;
	}

	return status;
}

/**
	This function is called when an instance is being deleted.
	FltObjects - Objects related to the filter, instance, and its associated volume.
	Flags - Flags that indicate the reason for the detach request.
*/
NTSTATUS
FilterTestingInstanceQueryTeardown (
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
	)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

	return STATUS_SUCCESS;
}

/**
	This function is called at the start of an instance teardown.
	FltObjects - Objects related to the filter, instance, and its associated volume.
	Flags - Flags that indicate the reason for the deletion.
*/
VOID
FilterTestingInstanceTeardownStart (
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
	)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();
}

/**
	This function is called at the end of an instance teardown.
	FltObjects - Objects related to the filter, instance, and its associated volume.
	Flags - Flags that indicate the reason for the deletion.
*/
VOID
FilterTestingInstanceTeardownComplete(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
	)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();
}

/**
	This function is called prior to a create operation.
	Data - The data associated with the current operation.
	FltObjects - Objects related to the filter, instance, and its associated volume.
	CompletionContext - Optional context to be passed to post operation callbacks.
*/
FLT_PREOP_CALLBACK_STATUS
FilterTestingPreCreateOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
)
{
	FLT_PREOP_CALLBACK_STATUS callbackStatus;
	PFLT_FILE_NAME_INFORMATION fileNameInfo;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	callbackStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

	if (FlagOn(Data->Iopb->Parameters.Create.Options, FILE_DELETE_ON_CLOSE)) {
		if (NT_SUCCESS(FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED, &fileNameInfo)))
		{
			if (fileFilters->MatchesFilter(fileNameInfo->Name.Buffer) != FALSE)
			{
				DBGPRINT("FilterTesting!FilterTestingPreCreateOperation: Detected FILE_DELETE_ON_CLOSE of %wZ. Prevented deletion!", fileNameInfo->Name);
				Data->Iopb->TargetFileObject->DeletePending = FALSE;
				Data->IoStatus.Information = 0;
				Data->IoStatus.Status = STATUS_ACCESS_DENIED;
				callbackStatus = FLT_PREOP_COMPLETE;
			}
		}
	}

    return callbackStatus;
}

/**
	This function is called prior to a write operation.
	Data - The data associated with the current operation.
	FltObjects - Objects related to the filter, instance, and its associated volume.
	CompletionContext - Optional context to be passed to post operation callbacks.
*/
FLT_PREOP_CALLBACK_STATUS
FilterTestingPreWriteOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
)
{
	FLT_PREOP_CALLBACK_STATUS callbackStatus;
	PFLT_FILE_NAME_INFORMATION fileNameInfo;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	callbackStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

	if (NT_SUCCESS(FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED, &fileNameInfo)))
	{
		if (fileFilters->MatchesFilter(fileNameInfo->Name.Buffer) != FALSE)
		{
			DBGPRINT("FilterTesting!FilterTestingPreCreateOperation: Detected write on %wZ. Prevented write!", fileNameInfo->Name);
			Data->Iopb->TargetFileObject->DeletePending = FALSE;
			Data->IoStatus.Information = 0;
			Data->IoStatus.Status = STATUS_ACCESS_DENIED;
			callbackStatus = FLT_PREOP_COMPLETE;
		}
	}

	return callbackStatus;
}

/**
	This function is called prior to a set information operation.
	Data - The data associated with the current operation.
	FltObjects - Objects related to the filter, instance, and its associated volume.
	CompletionContext - Optional context to be passed to post operation callbacks.
*/
FLT_PREOP_CALLBACK_STATUS
FilterTestingPreSetInfoOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
)
{
	FLT_PREOP_CALLBACK_STATUS callbackStatus;
	PFLT_FILE_NAME_INFORMATION fileNameInfo;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	callbackStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

	switch (Data->Iopb->Parameters.SetFileInformation.FileInformationClass) {
	case FileDispositionInformation:
	case FileDispositionInformationEx:
		if (NT_SUCCESS(FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED, &fileNameInfo)))
		{
			if (fileFilters->MatchesFilter(fileNameInfo->Name.Buffer) != FALSE)
			{
				DBGPRINT("FilterTesting!FilterTestingPreSetInfoOperation: Detected attempted file deletion of %wZ. Prevented deletion!", fileNameInfo->Name);
				Data->IoStatus.Information = 0;
				Data->IoStatus.Status = STATUS_ACCESS_DENIED;
				callbackStatus = FLT_PREOP_COMPLETE;
			}
		}
		break;
	}

	return callbackStatus;
}