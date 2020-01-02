#include "FSFilter.h"

FLT_REGISTRATION FSBlockingFilter::FilterRegistration;
PSTRING_FILTERS FSBlockingFilter::FileStringFilters;

/**
	Initializes the necessary components of the filesystem filter.
	@param DriverObject - The object of the driver necessary for mini-filter initialization.
	@param UnloadRoutine - The function to call on the unload of the mini-filter.
	@param InitializeStatus - Status of initialization.
	@param FilterHandle - The pointer to place the handle for the filter to.
*/
FSBlockingFilter::FSBlockingFilter (
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PFLT_FILTER_UNLOAD_CALLBACK UnloadRoutine,
	_Out_ NTSTATUS* InitializeStatus,
	_Out_ PFLT_FILTER* FilterHandle
	)
{

	FSBlockingFilter::FileStringFilters = new (PagedPool, STRING_FILE_FILTERS_TAG) StringFilters();
	if (FSBlockingFilter::FileStringFilters == NULL)
	{
		DBGPRINT("FSBlockingFilter!FSBlockingFilter: Failed to allocate memory for string filters.");
		*InitializeStatus = STATUS_NO_MEMORY;
		return;
	}

	//
	// This isn't a constant because the unload routine changes.
	//
	FSBlockingFilter::FilterRegistration = {
		sizeof(FLT_REGISTRATION),           //  Size
		FLT_REGISTRATION_VERSION,           //  Version
		0,                                  //  Flags

		NULL,                               //  Context
		Callbacks,                          //  Operation callbacks
		UnloadRoutine,

		FSBlockingFilter::HandleInstanceSetup,
		FSBlockingFilter::HandleInstanceQueryTeardown,
		FSBlockingFilter::HandleInstanceTeardownStart,
		FSBlockingFilter::HandleInstanceTeardownComplete,

		NULL,                               //  GenerateFileName
		NULL,                               //  GenerateDestinationFileName
		NULL                                //  NormalizeNameComponent
	};

	//
	//  Register with FltMgr to tell it our callback routines.
	//
	*InitializeStatus = FltRegisterFilter(DriverObject, &FilterRegistration, FilterHandle);

	FLT_ASSERT(NT_SUCCESS(*InitializeStatus));

	//
	// Start filtering.
	//
	*InitializeStatus = FltStartFiltering(*FilterHandle);

	//
	// If we can't start filtering, unregister the filter.
	//
	if (NT_SUCCESS(*InitializeStatus) == FALSE) {

		FltUnregisterFilter(*FilterHandle);
	}
}

/**
	Free data members that were dynamically allocated.
*/
FSBlockingFilter::~FSBlockingFilter()
{
	PAGED_CODE();

	DBGPRINT("FSBlockingFilter!~FSBlockingFilter: Deconstructing class.");
	//
	// Make sure to deconstruct the class.
	//
	if (FSBlockingFilter::FileStringFilters)
	{
		FSBlockingFilter::FileStringFilters->~StringFilters();
		ExFreePoolWithTag(FSBlockingFilter::FileStringFilters, STRING_FILE_FILTERS_TAG);
		FSBlockingFilter::FileStringFilters = NULL;
	}
}

/**
	Get the pointer to the filters used by this filesystem filter. Useful if you want to add/remove filters.
*/
PSTRING_FILTERS FSBlockingFilter::GetStringFilters()
{
	return FSBlockingFilter::FileStringFilters;
}

/**
	This function is called prior to a create operation.
	Data - The data associated with the current operation.
	FltObjects - Objects related to the filter, instance, and its associated volume.
	CompletionContext - Optional context to be passed to post operation callbacks.
*/
FLT_PREOP_CALLBACK_STATUS
FSBlockingFilter::HandlePreCreateOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
	)
{
	FLT_PREOP_CALLBACK_STATUS callbackStatus;
	PFLT_FILE_NAME_INFORMATION fileNameInfo;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	fileNameInfo = NULL;
	callbackStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

	if (FlagOn(Data->Iopb->Parameters.Create.Options, FILE_DELETE_ON_CLOSE)) {
		if (NT_SUCCESS(FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &fileNameInfo)))
		{
			if (FSBlockingFilter::FileStringFilters->MatchesFilter(fileNameInfo->Name.Buffer, FILTER_FLAG_DELETE) != FALSE)
			{
				DBGPRINT("FSBlockingFilter!HandlePreCreateOperation: Detected FILE_DELETE_ON_CLOSE of %wZ. Prevented deletion!", fileNameInfo->Name);
				Data->Iopb->TargetFileObject->DeletePending = FALSE;
				Data->IoStatus.Information = 0;
				Data->IoStatus.Status = STATUS_ACCESS_DENIED;
				callbackStatus = FLT_PREOP_COMPLETE;
			}
		}
	}

	if (fileNameInfo)
	{
		FltReleaseFileNameInformation(fileNameInfo);
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
FSBlockingFilter::HandlePreWriteOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
	)
{
	FLT_PREOP_CALLBACK_STATUS callbackStatus;
	PFLT_FILE_NAME_INFORMATION fileNameInfo;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	fileNameInfo = NULL;
	callbackStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

	if (NT_SUCCESS(FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &fileNameInfo)))
	{
		if (FSBlockingFilter::FileStringFilters->MatchesFilter(fileNameInfo->Name.Buffer, FILTER_FLAG_WRITE) != FALSE)
		{
			DBGPRINT("FSBlockingFilter!HandlePreWriteOperation: Detected write on %wZ. Prevented write!", fileNameInfo->Name);
			Data->Iopb->TargetFileObject->DeletePending = FALSE;
			Data->IoStatus.Information = 0;
			Data->IoStatus.Status = STATUS_ACCESS_DENIED;
			callbackStatus = FLT_PREOP_COMPLETE;
		}
	}

	if (fileNameInfo)
	{
		FltReleaseFileNameInformation(fileNameInfo);
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
FSBlockingFilter::HandlePreSetInfoOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
	)
{
	FLT_PREOP_CALLBACK_STATUS callbackStatus;
	PFLT_FILE_NAME_INFORMATION fileNameInfo;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	fileNameInfo = NULL;
	callbackStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

	switch (Data->Iopb->Parameters.SetFileInformation.FileInformationClass) {
	case FileDispositionInformation:
	case FileDispositionInformationEx:
		if (NT_SUCCESS(FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &fileNameInfo)))
		{
			if (FSBlockingFilter::FileStringFilters->MatchesFilter(fileNameInfo->Name.Buffer, FILTER_FLAG_DELETE) != FALSE)
			{
				DBGPRINT("FSBlockingFilter!HandlePreSetInfoOperation: Detected attempted file deletion of %wZ. Prevented deletion!", fileNameInfo->Name);
				Data->IoStatus.Information = 0;
				Data->IoStatus.Status = STATUS_ACCESS_DENIED;
				callbackStatus = FLT_PREOP_COMPLETE;
			}
		}
		break;
	}

	if (fileNameInfo)
	{
		FltReleaseFileNameInformation(fileNameInfo);
	}

	return callbackStatus;
}

/**
	This function determines whether or not the mini-filter should attach to the volume.
	FltObjects - Objects related to the filter, instance, and its associated volume.
	Flags - Flags that indicate the reason for the volume attach request.
	VolumeDeviceType - The device type of the specified volume.
	VolumeFilesystemType - The filesystem type of the specified volume.
*/
NTSTATUS
FSBlockingFilter::HandleInstanceSetup(
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
FSBlockingFilter::HandleInstanceQueryTeardown(
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
FSBlockingFilter::HandleInstanceTeardownStart(
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
FSBlockingFilter::HandleInstanceTeardownComplete(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
	)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();
}