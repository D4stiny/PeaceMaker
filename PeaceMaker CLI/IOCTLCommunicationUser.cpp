#include "IOCTLCommunicationUser.h"

/**
	Default constructor.
*/
IOCTLCommunication::IOCTLCommunication(
	VOID
	)
{
	
}

/**
	Disconnect from the driver.
*/
IOCTLCommunication::~IOCTLCommunication(
	VOID
	)
{
	CloseHandle(this->device);
}

/**
	Establish communication with the driver.
	@return Whether or not we successfully connected to the driver.
*/
BOOLEAN
IOCTLCommunication::ConnectDevice(
	VOID
	)
{
	this->device = CreateFile(GLOBAL_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (this->device == INVALID_HANDLE_VALUE)
	{
		printf("IOCTLCommunication!IOCTLCommunication: Failed to connect to driver device with error %i.\n", GetLastError());
		return FALSE;
	}

	printf("IOCTLCommunication!IOCTLCommunication: Established communication with driver.\n");
	return TRUE;
}

/**
	Small wrapper around the DeviceIoControl call to the driver.
	@param IOCTLCode - The IOCTL code to pass to the driver.
	@param Input - The input buffer.
	@param InputLength - Size of the input buffer.
	@param Output - The output buffer.
	@param OutputLength - Size of the output buffer.
	@return Whether or not the call succeeded.
*/
BOOLEAN
IOCTLCommunication::GenericQueryDriver (
	_In_ DWORD IOCTLCode,
	_In_ PVOID Input,
	_In_ DWORD InputLength,
	_Out_ PVOID Output,
	_In_ DWORD OutputLength
	)
{
	DWORD bytesReturned;
	return DeviceIoControl(this->device, IOCTLCode, Input, InputLength, Output, OutputLength, &bytesReturned, NULL);
}

/**
	Query if there are alerts queued.
	@return Whether or not alerts are queued.
*/
BOOLEAN
IOCTLCommunication::QueuedAlerts (
	VOID
	)
{
	BOOLEAN queued;

	//
	// Query the driver passing in an output boolean.
	//
	if (this->GenericQueryDriver(IOCTL_ALERTS_QUEUED, NULL, 0, &queued, sizeof(queued)) == FALSE)
	{
		printf("IOCTLCommunication!QueuedAlerts: Failed to query driver with error code %i.\n", GetLastError());
		return FALSE;
	}

	return queued;
}

/**
	Pop a queued alert from the Alerts queue.
	@return An allocated pointer to the alert or NULL if unable to pop an alert. Caller must free to prevent leak.
*/
PBASE_ALERT_INFO
IOCTLCommunication::PopAlert(
	VOID
	)
{
	BOOLEAN success;
	PBASE_ALERT_INFO alert;

	success = FALSE;

	alert = RCAST<PBASE_ALERT_INFO>(malloc(MAX_STACK_VIOLATION_ALERT_SIZE));
	if (alert == NULL)
	{
		printf("IOCTLCommunication!PopAlert: Failed to allocate space for alert.\n");
		goto Exit;
	}
	memset(alert, 0, MAX_STACK_VIOLATION_ALERT_SIZE);

	if (this->GenericQueryDriver(IOCTL_POP_ALERT, NULL, 0, alert, MAX_STACK_VIOLATION_ALERT_SIZE) == FALSE)
	{
		printf("IOCTLCommunication!PopAlert: Failed to query driver with error code %i.\n", GetLastError());
		goto Exit;
	}
	success = TRUE;
Exit:
	if (success == FALSE && alert)
	{
		free(alert);
		alert = NULL;
	}
	return alert;
}

/**
	Request a summary of the most recent processes up to RequestCount.
	@param SkipCount - How many processes to "skip" in iteration.
	@param RequestCount - How many proocesses to get the summary of.
	@return The response to the summary request if any, otherwise NULL. Caller must free to prevent leak.
*/
PPROCESS_SUMMARY_REQUEST
IOCTLCommunication::RequestProcessSummary (
	_In_ ULONG SkipCount,
	_In_ ULONG RequestCount
	)
{
	BOOLEAN success;
	PPROCESS_SUMMARY_REQUEST summaryRequest;
	ULONG summaryRequestSize;

	success = FALSE;
	summaryRequestSize = MAX_PROCESS_SUMMARY_REQUEST_SIZE_RAW(RequestCount);

	summaryRequest = RCAST<PPROCESS_SUMMARY_REQUEST>(malloc(summaryRequestSize));
	if (summaryRequest == NULL)
	{
		printf("IOCTLCommunication!RequestProcessSummary: Failed to allocate space for summaryRequest.\n");
		goto Exit;
	}
	memset(summaryRequest, 0, summaryRequestSize);

	summaryRequest->SkipCount = SkipCount;
	summaryRequest->ProcessHistorySize = RequestCount;

	if (this->GenericQueryDriver(IOCTL_GET_PROCESSES, summaryRequest, summaryRequestSize, summaryRequest, summaryRequestSize) == FALSE)
	{
		printf("IOCTLCommunication!RequestProcessSummary: Failed to query driver with error code %i.\n", GetLastError());
		goto Exit;
	}
	success = TRUE;
Exit:
	if (success == FALSE && summaryRequest)
	{
		free(summaryRequest);
		summaryRequest = NULL;
	}
	return summaryRequest;
}

/**
	Request detailed information on a process.
	@param ProcessId - The subject process.
	@param EpochExecutionTime - The time the process was executed (in seconds since epoch).
	@param MaxImageSize - The maximum number of image entries to copy.
	@param MaxStackSize - The maximum number of stack entries to copy.
	@return The response to the detailed request if any, otherwise NULL. Caller must free to prevent leak.
*/
PPROCESS_DETAILED_REQUEST
IOCTLCommunication::RequestDetailedProcess (
	_In_ HANDLE ProcessId,
	_In_ ULONG EpochExecutionTime,
	_In_ ULONG MaxImageSize,
	_In_ ULONG MaxStackSize
	)
{
	BOOLEAN success;
	PPROCESS_DETAILED_REQUEST detailedRequest;
	ULONG detailedRequestSize;

	success = FALSE;
	detailedRequestSize = sizeof(PROCESS_DETAILED_REQUEST);

	//
	// Allocate the necessary members.
	//
	detailedRequest = RCAST<PPROCESS_DETAILED_REQUEST>(malloc(detailedRequestSize));
	if (detailedRequest == NULL)
	{
		printf("IOCTLCommunication!RequestDetailedProcess: Failed to allocate space for detailedRequest.\n");
		goto Exit;
	}
	memset(detailedRequest, 0, detailedRequestSize);

	detailedRequest->ImageSummary = RCAST<PIMAGE_SUMMARY>(malloc(MaxImageSize * sizeof(IMAGE_SUMMARY)));
	if (detailedRequest->ImageSummary == NULL)
	{
		printf("IOCTLCommunication!RequestDetailedProcess: Failed to allocate space for detailedRequest->ImageSummary.\n");
		goto Exit;
	}
	memset(detailedRequest->ImageSummary, 0, MaxImageSize * sizeof(IMAGE_SUMMARY));

	detailedRequest->StackHistory = RCAST<PSTACK_RETURN_INFO>(malloc(MaxStackSize * sizeof(STACK_RETURN_INFO)));
	if (detailedRequest->StackHistory == NULL)
	{
		printf("IOCTLCommunication!RequestDetailedProcess: Failed to allocate space for detailedRequest->StackHistory.\n");
		goto Exit;
	}
	memset(detailedRequest->StackHistory, 0, MaxStackSize * sizeof(STACK_RETURN_INFO));

	detailedRequest->ProcessId = ProcessId;
	detailedRequest->EpochExecutionTime = EpochExecutionTime;
	detailedRequest->ImageSummarySize = MaxImageSize;
	detailedRequest->StackHistorySize = MaxStackSize;

	if (this->GenericQueryDriver(IOCTL_GET_PROCESS_DETAILED, detailedRequest, detailedRequestSize, detailedRequest, detailedRequestSize) == FALSE)
	{
		printf("IOCTLCommunication!RequestDetailedProcess: Failed to query driver with error code %i.\n", GetLastError());
		goto Exit;
	}
	success = TRUE;
Exit:
	if (success == FALSE && detailedRequest)
	{
		free(detailedRequest);
		detailedRequest = NULL;
	}
	return detailedRequest;
}

/**
	Register a filter with the driver.
	@param Type - The filter type.
	@param Flags - The filter flags (EXECUTE/DELETE/WRITE/ETC).
	@param Content - The content of the filter.
	@param ContentLength - The size of the content.
	@return The filter ID (if added successfully), otherwise 0.
*/
ULONG
IOCTLCommunication::AddFilter (
	_In_ STRING_FILTER_TYPE Type,
	_In_ ULONG Flags,
	_In_ PWCHAR Content,
	_In_ ULONG ContentLength
	)
{

	STRING_FILTER_REQUEST filterRequest;

	//
	// Fill out the struct.
	//
	filterRequest.FilterType = Type;
	filterRequest.Filter.Flags = Flags;
	memcpy_s(filterRequest.Filter.MatchString, sizeof(filterRequest.Filter.MatchString), Content, ContentLength * sizeof(WCHAR));
	filterRequest.Filter.MatchStringSize = ContentLength;

	//
	// Query the driver passing in the filter request.
	//
	if (this->GenericQueryDriver(IOCTL_ADD_FILTER, &filterRequest, sizeof(filterRequest), &filterRequest, sizeof(filterRequest)) == FALSE)
	{
		printf("IOCTLCommunication!AddFilter: Failed to query driver with error code %i.\n", GetLastError());
		return 0;
	}

	return filterRequest.Filter.Id;
}

/**
	Request a list of filters with a constant size of 10. Grab more by using the SkipCount argument.
	@param Type - The type of filters to grab.
	@param SkipCount - The number of filters to skip during iteration.
	@return The response to the request.
*/
LIST_FILTERS_REQUEST
IOCTLCommunication::RequestFilters(
	_In_ STRING_FILTER_TYPE Type,
	_In_ ULONG SkipCount
	)
{
	LIST_FILTERS_REQUEST listRequest;

	listRequest.FilterType = Type;
	listRequest.SkipFilters = SkipCount;

	//
	// Query the driver passing in the list request.
	//
	if (this->GenericQueryDriver(IOCTL_LIST_FILTERS, &listRequest, sizeof(listRequest), &listRequest, sizeof(listRequest)) == FALSE)
	{
		printf("IOCTLCommunication!RequestFilters: Failed to query driver with error code %i.\n", GetLastError());
	}

	return listRequest;
}

/**
	Get the dynamic sizes in a process entry from the driver.
	@param ProcessId - The target process ID.
	@param EpochExecutionTime - The time the target process was executed.
	@return The response.
*/
PROCESS_SIZES_REQUEST
IOCTLCommunication::GetProcessSizes(
	_In_ HANDLE ProcessId,
	_In_ ULONG EpochExecutionTime
	)
{
	PROCESS_SIZES_REQUEST sizeRequest;

	sizeRequest.ProcessId = ProcessId;
	sizeRequest.EpochExecutionTime = EpochExecutionTime;

	//
	// Query the driver passing in the list request.
	//
	if (this->GenericQueryDriver(IOCTL_GET_PROCESS_SIZES, &sizeRequest, sizeof(sizeRequest), &sizeRequest, sizeof(sizeRequest)) == FALSE)
	{
		printf("IOCTLCommunication!GetProcessSizes: Failed to query driver with error code %i.\n", GetLastError());
	}

	return sizeRequest;
}

/**
	Request detailed information on an image.
	@param ProcessId - The subject process.
	@param EpochExecutionTime - The time the process was executed (in seconds since epoch).
	@param MaxStackSize - The maximum number of stack entries to copy.
	@return The response to the detailed request if any, otherwise NULL. Caller must free to prevent leak.
*/
PIMAGE_DETAILED_REQUEST
IOCTLCommunication::RequestDetailedImage (
	_In_ HANDLE ProcessId,
	_In_ ULONG EpochExecutionTime,
	_In_ ULONG ImageIndex,
	_In_ ULONG MaxStackSize
	)
{
	BOOLEAN success;
	PIMAGE_DETAILED_REQUEST imageRequest;
	ULONG imageRequestSize;

	success = FALSE;
	imageRequestSize = MAX_IMAGE_DETAILED_REQUEST_SIZE_RAW(MaxStackSize);

	imageRequest = RCAST<PIMAGE_DETAILED_REQUEST>(malloc(imageRequestSize));
	if (imageRequest == NULL)
	{
		printf("IOCTLCommunication!RequestDetailedImage: Failed to allocate space for imageRequest.\n");
		goto Exit;
	}
	memset(imageRequest, 0, imageRequestSize);

	imageRequest->ProcessId = ProcessId;
	imageRequest->EpochExecutionTime = EpochExecutionTime;
	imageRequest->ImageIndex = ImageIndex;
	imageRequest->StackHistorySize = MaxStackSize;

	if (this->GenericQueryDriver(IOCTL_GET_IMAGE_DETAILED, imageRequest, imageRequestSize, imageRequest, imageRequestSize) == FALSE)
	{
		printf("IOCTLCommunication!RequestDetailedImage: Failed to query driver with error code %i.\n", GetLastError());
		goto Exit;
	}
	success = TRUE;
Exit:
	if (success == FALSE && imageRequest)
	{
		free(imageRequest);
		imageRequest = NULL;
	}
	return imageRequest;
}

/**
	Get global sizes from the kernel
	@return The various sizes of data stored in the kernel.
*/
GLOBAL_SIZES
IOCTLCommunication::GetGlobalSizes (
	VOID
	)
{
	GLOBAL_SIZES sizes;

	//
	// Query the driver passing in the size request.
	//
	if (this->GenericQueryDriver(IOCTL_GET_GLOBAL_SIZES, NULL, 0, &sizes, sizeof(sizes)) == FALSE)
	{
		printf("IOCTLCommunication!GetProcessSizes: Failed to query driver with error code %i.\n", GetLastError());
	}

	return sizes;
}

/**
	Delete a filter.
	@param Filter - The filter to delete.
	@return Whether or not the filter was deleted.
*/
BOOLEAN
IOCTLCommunication::DeleteFilter (
	_In_ FILTER_INFO Filter
	)
{
	DELETE_FILTER_REQUEST deleteFilterRequest;

	deleteFilterRequest.FilterId = Filter.Id;
	deleteFilterRequest.FilterType = Filter.Type;

	//
	// Query the driver passing in the delete request.
	//
	if (this->GenericQueryDriver(IOCTL_DELETE_FILTER, &deleteFilterRequest, sizeof(deleteFilterRequest), &deleteFilterRequest, sizeof(deleteFilterRequest)) == FALSE)
	{
		printf("IOCTLCommunication!DeleteFilter: Failed to query driver with error code %i.\n", GetLastError());
	}

	return deleteFilterRequest.Deleted;
}