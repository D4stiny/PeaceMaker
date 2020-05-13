/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 * 
 * COPYRIGHT Bill Demirkapi 2020
 */
#include "IOCTLCommunication.h"

PDRIVER_OBJECT IOCTLCommunication::DriverObject;
PDETECTION_LOGIC IOCTLCommunication::Detector;
PIMAGE_HISTORY_FILTER IOCTLCommunication::ImageProcessFilter;
PFLT_FILTER IOCTLCommunication::FileFilterHandle;
PFS_BLOCKING_FILTER IOCTLCommunication::FilesystemMonitor;
PREGISTRY_BLOCKING_FILTER IOCTLCommunication::RegistryMonitor;
PTHREAD_FILTER IOCTLCommunication::ThreadOperationFilter;
PTAMPER_GUARD IOCTLCommunication::TamperGuardFilter;

/**
	Construct the IOCTLCommunication class by initializing the driver object and detector.
	@param DriverObject - The driver's object.
	@param RegistryPath - The registry path of the driver.
	@param UnloadRoutine - The routine to call when the filter is unloading.
	@param InitializeStatus - Status of initialization.
*/
IOCTLCommunication::IOCTLCommunication (
	_In_ PDRIVER_OBJECT Driver,
	_In_ PUNICODE_STRING RegistryPath,
	_In_ PFLT_FILTER_UNLOAD_CALLBACK UnloadRoutine,
	_Inout_ NTSTATUS* InitializeStatus
	)
{
	this->DriverObject = Driver;

	*InitializeStatus = STATUS_SUCCESS;

	//
	// Initialize the class members.
	//
	this->Detector = new (NonPagedPool, DETECTION_LOGIC_TAG) DetectionLogic();
	if (this->Detector == NULL)
	{
		DBGPRINT("IOCTLCommunication!IOCTLCommunication: Failed to allocate space for detection logic.");
		*InitializeStatus = STATUS_NO_MEMORY;
		return;
	}

	this->ImageProcessFilter = new (NonPagedPool, IMAGE_HISTORY_FILTER_TAG) ImageHistoryFilter(this->Detector, InitializeStatus);
	if (NT_SUCCESS(*InitializeStatus) == FALSE)
	{
		DBGPRINT("IOCTLCommunication!IOCTLCommunication: Failed to initialize image process history filter with status 0x%X.", *InitializeStatus);
		return;
	}
	if (this->ImageProcessFilter == NULL)
	{
		DBGPRINT("IOCTLCommunication!IOCTLCommunication: Failed to allocate space for image process history filter.");
		*InitializeStatus = STATUS_NO_MEMORY;
		return;
	}

	FilesystemMonitor = new (NonPagedPool, FILE_MONITOR_TAG) FSBlockingFilter(DriverObject, RegistryPath, UnloadRoutine, this->Detector, InitializeStatus, &FileFilterHandle);
	if (NT_SUCCESS(*InitializeStatus) == FALSE)
	{
		DBGPRINT("IOCTLCommunication!IOCTLCommunication: Failed to initialize the filesystem blocking filter with status 0x%X.", *InitializeStatus);
		return;
	}

	RegistryMonitor = new (NonPagedPool, REGISTRY_MONITOR_TAG) RegistryBlockingFilter(DriverObject, RegistryPath, this->Detector, InitializeStatus);
	if (NT_SUCCESS(*InitializeStatus) == FALSE)
	{
		DBGPRINT("IOCTLCommunication!IOCTLCommunication: Failed to initialize the registry blocking filter with status 0x%X.", *InitializeStatus);
		return;
	}

	this->ThreadOperationFilter = new (NonPagedPool, THREAD_FILTER_TAG) ThreadFilter(this->Detector, InitializeStatus);
	if (NT_SUCCESS(*InitializeStatus) == FALSE)
	{
		DBGPRINT("IOCTLCommunication!IOCTLCommunication: Failed to initialize thread operation filters with status 0x%X.", *InitializeStatus);
		return;
	}
	if (this->ThreadOperationFilter == NULL)
	{
		DBGPRINT("IOCTLCommunication!IOCTLCommunication: Failed to allocate space for thread operation filters.");
		*InitializeStatus = STATUS_NO_MEMORY;
		return;
	}

	this->TamperGuardFilter = new (NonPagedPool, TAMPER_GUARD_TAG) TamperGuard(InitializeStatus);
	if (NT_SUCCESS(*InitializeStatus) == FALSE)
	{
		DBGPRINT("IOCTLCommunication!IOCTLCommunication: Failed to initialize tamper guard with status 0x%X.", *InitializeStatus);
		return;
	}

	InitializeDriverIOCTL();
}

/**
	Deconstruct the IOCTLCommunication class.
*/
IOCTLCommunication::~IOCTLCommunication	(
	VOID
	)
{
	this->Detector->~DetectionLogic();
	ExFreePoolWithTag(this->Detector, DETECTION_LOGIC_TAG);

	this->ImageProcessFilter->~ImageHistoryFilter();
	ExFreePoolWithTag(this->ImageProcessFilter, IMAGE_HISTORY_FILTER_TAG);

	FltUnregisterFilter(FileFilterHandle);

	this->FilesystemMonitor->~FSBlockingFilter();
	ExFreePoolWithTag(this->FilesystemMonitor, FILE_MONITOR_TAG);

	this->RegistryMonitor->~RegistryBlockingFilter();
	ExFreePoolWithTag(this->RegistryMonitor, REGISTRY_MONITOR_TAG);

	this->ThreadOperationFilter->~ThreadFilter();
	ExFreePoolWithTag(this->ThreadOperationFilter, THREAD_FILTER_TAG);

	this->TamperGuardFilter->~TamperGuard();
	ExFreePoolWithTag(this->TamperGuardFilter, TAMPER_GUARD_TAG);

	UninitializeDriverIOCTL();
}

/**
	Handle basic create / close of device, always return success with no change.
	@param DeviceObject - The driver's device object.
	@param Irp - The current IRP.
*/
NTSTATUS
IOCTLCommunication::IOCTLCreateClose (
	_In_ PDEVICE_OBJECT DeviceObject,
	_In_ PIRP Irp
	)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	//
	// Just accept everyone for now.
	// TODO: Implement some sort of authentication?
	//
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

/**
	Handle IO controls for communication with the PeaceMaker user-mode interface.
	@param DeviceObject - The driver's device object.
	@param Irp - The current IRP.
*/
NTSTATUS
IOCTLCommunication::IOCTLDeviceControl (
	_In_ PDEVICE_OBJECT DeviceObject,
	_In_ PIRP Irp
	)
{
	NTSTATUS status;
	PIO_STACK_LOCATION irpStackLocation;
	ULONG ioctlCode;
	ULONG inputLength;
	ULONG outputLength;
	ULONG minimumLength;
	ULONG writtenLength;

	PBASE_ALERT_INFO poppedAlert;
	PPROCESS_SUMMARY_REQUEST processSummaryRequest;
	PPROCESS_DETAILED_REQUEST processDetailedRequest;
	PSTRING_FILTER_REQUEST filterAddRequest;
	PLIST_FILTERS_REQUEST listFiltersRequest;
	PIMAGE_DETAILED_REQUEST imageDetailedRequest;
	PDELETE_FILTER_REQUEST deleteFilterRequest;
	PGLOBAL_SIZES globalSizes;

	WCHAR temporaryFilterBuffer[MAX_PATH];

	UNREFERENCED_PARAMETER(DeviceObject);

	status = STATUS_SUCCESS;
	irpStackLocation = IoGetCurrentIrpStackLocation(Irp);

	//
	// Grab basic information about the request.
	//
	ioctlCode = irpStackLocation->Parameters.DeviceIoControl.IoControlCode;
	inputLength = irpStackLocation->Parameters.DeviceIoControl.InputBufferLength;
	outputLength = irpStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
	writtenLength = 0;
	
	//
	// Update the tamper guard.
	//
	IOCTLCommunication::TamperGuardFilter->UpdateProtectedProcess(PsGetCurrentProcessId());

	DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: ioctlCode = 0x%X, inputLength = 0x%X, outputLength = 0x%X", ioctlCode, inputLength, outputLength);

	//
	// Handle the different IOCTL request types.
	//
	switch (ioctlCode)
	{
	case IOCTL_ALERTS_QUEUED:
		if (outputLength >= sizeof(BOOLEAN))
		{
			//
			// Return the status of the Queue.
			//
			*RCAST<BOOLEAN*>(Irp->AssociatedIrp.SystemBuffer) = !IOCTLCommunication::Detector->GetAlertQueue()->IsQueueEmpty();
			writtenLength = sizeof(BOOLEAN);
		}
		break;
	case IOCTL_POP_ALERT:
		if (outputLength < MAX_STACK_VIOLATION_ALERT_SIZE)
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_POP_ALERT but output buffer with size 0x%X smaller then minimum 0x%X.", outputLength, MAX_STACK_VIOLATION_ALERT_SIZE);
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto Exit;
		}
		//
		// Pop an alert from the queue.
		//
		poppedAlert = IOCTLCommunication::Detector->GetAlertQueue()->PopAlert();
		if (poppedAlert == NULL)
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_POP_ALERT but no alert to pop.");
			status = STATUS_NOT_FOUND;
			goto Exit;
		}

		DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Got alert 0x%llx for IOCTL_POP_ALERT with size 0x%llx.\n", poppedAlert, poppedAlert->AlertSize);

		//
		// Copy the alert.
		//
		memcpy_s(Irp->AssociatedIrp.SystemBuffer, outputLength, poppedAlert, poppedAlert->AlertSize);

		writtenLength = poppedAlert->AlertSize;

		//
		// Free the alert entry.
		//
		IOCTLCommunication::Detector->GetAlertQueue()->FreeAlert(poppedAlert);
		break;
	case IOCTL_GET_PROCESSES:
		if (inputLength < sizeof(PROCESS_SUMMARY_REQUEST))
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_GET_PROCESSES but input buffer is too small.");
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto Exit;
		}

		//
		// Verify the specified array size.
		//
		processSummaryRequest = RCAST<PPROCESS_SUMMARY_REQUEST>(Irp->AssociatedIrp.SystemBuffer);
		if (processSummaryRequest->ProcessHistorySize <= 0 || outputLength < MAX_PROCESS_SUMMARY_REQUEST_SIZE(processSummaryRequest))
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_GET_PROCESSES but output buffer with size 0x%X smaller then minimum 0x%X.", outputLength, MAX_PROCESS_SUMMARY_REQUEST_SIZE(processSummaryRequest));
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto Exit;
		}

		//
		// Grab the history summaries.
		//
		processSummaryRequest->ProcessHistorySize = IOCTLCommunication::ImageProcessFilter->GetProcessHistorySummary(processSummaryRequest->SkipCount, RCAST<PPROCESS_SUMMARY_ENTRY>(&processSummaryRequest->ProcessHistory[0]), processSummaryRequest->ProcessHistorySize);
		writtenLength = MAX_PROCESS_SUMMARY_REQUEST_SIZE(processSummaryRequest);

		DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: IOCTL_GET_PROCESSES found %i processes.", processSummaryRequest->ProcessHistorySize);
		break;
	case IOCTL_GET_PROCESS_DETAILED:
		if (inputLength < sizeof(PROCESS_DETAILED_REQUEST))
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_GET_PROCESS_DETAILED but input buffer is too small.");
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto Exit;
		}

		processDetailedRequest = RCAST<PPROCESS_DETAILED_REQUEST>(Irp->AssociatedIrp.SystemBuffer);

		minimumLength = sizeof(PROCESS_DETAILED_REQUEST);
		//
		// Verify the specified array size.
		//
		if (outputLength < minimumLength)
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_GET_PROCESS_DETAILED but output buffer with size 0x%X smaller then minimum 0x%X.", outputLength, minimumLength);
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto Exit;
		}

		//
		// Verify the user buffers.
		//
		__try
		{
			ProbeForWrite(processDetailedRequest->ImageSummary, processDetailedRequest->ImageSummarySize * sizeof(IMAGE_SUMMARY), sizeof(ULONG));
			ProbeForWrite(processDetailedRequest->StackHistory, processDetailedRequest->StackHistorySize * sizeof(STACK_RETURN_INFO), sizeof(ULONG));
		}
		__except (1)
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_GET_PROCESS_DETAILED but user buffers were invalid.");
			status = STATUS_BAD_DATA;
			goto Exit;
		}
		
		//
		// Populate the detailed request.
		//
		IOCTLCommunication::ImageProcessFilter->PopulateProcessDetailedRequest(processDetailedRequest);
		writtenLength = minimumLength;
		break;
	case IOCTL_ADD_FILTER:
		//
		// Validate the size of the input and output buffers.
		//
		if (inputLength < sizeof(STRING_FILTER_REQUEST))
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_ADD_FILTER but input buffer is too small.");
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto Exit;
		}
		if (outputLength < sizeof(STRING_FILTER_REQUEST))
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_ADD_FILTER but output buffer is too small.");
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto Exit;
		}

		filterAddRequest = RCAST<PSTRING_FILTER_REQUEST>(Irp->AssociatedIrp.SystemBuffer);

		//
		// Copy the filter content to a temporary string (ensures null-terminator).
		//
		status = RtlStringCchCopyNW(temporaryFilterBuffer, MAX_PATH, filterAddRequest->Filter.MatchString, MAX_PATH);
		if (NT_SUCCESS(status) == FALSE)
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Failed to copy filter content to temporary buffer with status 0x%X.", status);
			goto Exit;
		}

		//
		// Sanity check.
		//
		if (wcsnlen_s(temporaryFilterBuffer, MAX_PATH) == 0)
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Blocked empty filter.");
			goto Exit;
		}
		
		//
		// Depending on the type of filter, add the string.
		//
		switch (filterAddRequest->FilterType)
		{
		case FilesystemFilter:
			filterAddRequest->Filter.Id = FilesystemMonitor->GetStringFilters()->AddFilter(temporaryFilterBuffer, filterAddRequest->Filter.Flags);
			break;
		case RegistryFilter:
			filterAddRequest->Filter.Id = RegistryMonitor->GetStringFilters()->AddFilter(temporaryFilterBuffer, filterAddRequest->Filter.Flags);
			break;
		}
		writtenLength = sizeof(STRING_FILTER_REQUEST);
		break;
	case IOCTL_LIST_FILTERS:
		//
		// Validate the size of the input and output buffers.
		//
		if (inputLength < sizeof(LIST_FILTERS_REQUEST))
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_LIST_FILTERS but input buffer is too small.");
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto Exit;
		}
		if (outputLength < sizeof(LIST_FILTERS_REQUEST))
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_LIST_FILTERS but output buffer is too small.");
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto Exit;
		}

		listFiltersRequest = RCAST<PLIST_FILTERS_REQUEST>(Irp->AssociatedIrp.SystemBuffer);
		switch (listFiltersRequest->FilterType)
		{
		case FilesystemFilter:
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Retrieving filesystem filters.");
			listFiltersRequest->CopiedFilters = FilesystemMonitor->GetStringFilters()->GetFilters(listFiltersRequest->SkipFilters, RCAST<PFILTER_INFO>(&listFiltersRequest->Filters), 10);
			break;
		case RegistryFilter:
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Retrieving registry filters.");
			listFiltersRequest->CopiedFilters = RegistryMonitor->GetStringFilters()->GetFilters(listFiltersRequest->SkipFilters, RCAST<PFILTER_INFO>(&listFiltersRequest->Filters), 10);
			break;
		}
		writtenLength = sizeof(LIST_FILTERS_REQUEST);
		break;
	case IOCTL_GET_PROCESS_SIZES:
		//
		// Validate the size of the input and output buffers.
		//
		if (inputLength < sizeof(PROCESS_SIZES_REQUEST))
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_GET_PROCESS_SIZES but input buffer is too small.");
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto Exit;
		}
		if (outputLength < sizeof(PROCESS_SIZES_REQUEST))
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_GET_PROCESS_SIZES but output buffer is too small.");
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto Exit;
		}

		IOCTLCommunication::ImageProcessFilter->PopulateProcessSizes(RCAST<PPROCESS_SIZES_REQUEST>(Irp->AssociatedIrp.SystemBuffer));
		writtenLength = sizeof(PROCESS_SIZES_REQUEST);
		break;
	case IOCTL_GET_IMAGE_DETAILED:
		//
		// Validate the size of the input and output buffers.
		//
		if (inputLength < sizeof(IMAGE_DETAILED_REQUEST))
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_GET_IMAGE_DETAILED but input buffer is too small.");
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto Exit;
		}

		imageDetailedRequest = RCAST<PIMAGE_DETAILED_REQUEST>(Irp->AssociatedIrp.SystemBuffer);
		minimumLength = MAX_IMAGE_DETAILED_REQUEST_SIZE(imageDetailedRequest);
		DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: IOCTL_GET_IMAGE_DETAILED minimumLength = 0x%X.", minimumLength);

		if (inputLength < minimumLength)
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_GET_IMAGE_DETAILED but input buffer is too small.");
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto Exit;
		}
		if (outputLength < minimumLength)
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_GET_IMAGE_DETAILED but output buffer is too small.");
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto Exit;
		}
		IOCTLCommunication::ImageProcessFilter->PopulateImageDetailedRequest(imageDetailedRequest);
		writtenLength = MAX_IMAGE_DETAILED_REQUEST_SIZE(imageDetailedRequest);
		break;
	case IOCTL_GET_GLOBAL_SIZES:
		//
		// Validate the size of the output buffer.
		//
		if (outputLength < sizeof(GLOBAL_SIZES))
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_GET_GLOBAL_SIZES but output buffer is too small.");
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto Exit;
		}

		globalSizes = RCAST<PGLOBAL_SIZES>(Irp->AssociatedIrp.SystemBuffer);
		globalSizes->ProcessHistorySize = ImageHistoryFilter::ProcessHistorySize;
		globalSizes->FilesystemFilterSize = FilesystemMonitor->GetStringFilters()->filtersCount;
		globalSizes->RegistryFilterSize = RegistryMonitor->GetStringFilters()->filtersCount;
		writtenLength = sizeof(GLOBAL_SIZES);
		break;
	case IOCTL_DELETE_FILTER:
		//
		// Validate the size of the input buffer.
		//
		if (inputLength < sizeof(DELETE_FILTER_REQUEST))
		{
			DBGPRINT("IOCTLCommunication!IOCTLDeviceControl: Received IOCTL_DELETE_FILTER but input buffer is too small.");
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto Exit;
		}

		deleteFilterRequest = RCAST<PDELETE_FILTER_REQUEST>(Irp->AssociatedIrp.SystemBuffer);
		switch (deleteFilterRequest->FilterType)
		{
		case FilesystemFilter:
			deleteFilterRequest->Deleted = FilesystemMonitor->GetStringFilters()->RemoveFilter(deleteFilterRequest->FilterId);
			break;
		case RegistryFilter:
			deleteFilterRequest->Deleted = RegistryMonitor->GetStringFilters()->RemoveFilter(deleteFilterRequest->FilterId);
			break;
		}
		writtenLength = sizeof(DELETE_FILTER_REQUEST);
		break;
	}

Exit:
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = writtenLength;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}

/**
	Initialize the driver object to support IOCTL communication.
*/
NTSTATUS
IOCTLCommunication::InitializeDriverIOCTL (
	VOID
	)
{
	NTSTATUS status;
	UNICODE_STRING ioctlDeviceName;
	UNICODE_STRING ioctlDosDevicesName;
	PDEVICE_OBJECT ioctlDevice;

	RtlInitUnicodeString(&ioctlDeviceName, NT_DEVICE_NAME);

	//
	// Create IO Device Object.
	// TODO: Implement secure device creation (with secure DACL).
	//
	status = IoCreateDevice(DriverObject, NULL, &ioctlDeviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, TRUE, &ioctlDevice);
	if (NT_SUCCESS(status) == FALSE)
	{
		DBGPRINT("IOCTLCommunication!InitializeDriverIOCTL: Failed to create kernel device object with error 0x%X.", status);
		goto Exit;
	}

	//
	// Set the handlers for our IOCTL.
	//
	DriverObject->MajorFunction[IRP_MJ_CREATE] = IOCTLCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = IOCTLCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IOCTLDeviceControl;

	RtlInitUnicodeString(&ioctlDosDevicesName, DOS_DEVICE_NAME);

	status = IoCreateSymbolicLink(&ioctlDosDevicesName, &ioctlDeviceName);
	if (NT_SUCCESS(status) == FALSE)
	{
		DBGPRINT("IOCTLCommunication!InitializeDriverIOCTL: Failed to create symbolic link to device with error 0x%X.", status);
		IoDeleteDevice(ioctlDevice);
		goto Exit;
	}
Exit:
	return status;
}

/**
	Undo everything done in InitializeDriverObject.
	@return The status of uninitialization.
*/
VOID
IOCTLCommunication::UninitializeDriverIOCTL (
	VOID
	)
{
	PDEVICE_OBJECT deviceObject;
	UNICODE_STRING ioctlDosDevicesName;

	deviceObject = DriverObject->DeviceObject;

	//
	// Initialize the unicode string of our DosDevices symlink.
	//
	RtlInitUnicodeString(&ioctlDosDevicesName, DOS_DEVICE_NAME);

	//
	// Delete IOCTL symlink because we're unloading.
	//
	IoDeleteSymbolicLink(&ioctlDosDevicesName);
	if (deviceObject != NULL)
	{
		//
		// Delete the device while unloading.
		//
		IoDeleteDevice(deviceObject);
	}
}