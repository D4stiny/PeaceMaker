#include "ImageHistoryFilter.h"

/**
	Register the necessary notify routines.
	@param InitializeStatus - Status of initialization.
*/
ImageHistoryFilter::ImageHistoryFilter (
	_Out_ NTSTATUS* InitializeStatus
	)
{
	//
	// Set the create process notify routine.
	//
	*InitializeStatus = PsSetCreateProcessNotifyRoutine(ImageHistoryFilter::CreateProcessNotifyRoutine, FALSE);
	if (NT_SUCCESS(*InitializeStatus) == FALSE)
	{
		DBGPRINT("ImageHistoryFilter!ImageHistoryFilter: Failed to register create process notify routine with status 0x%X.", *InitializeStatus);
		return;
	}
	
	//
	// Set the load image notify routine.
	//
	*InitializeStatus = PsSetLoadImageNotifyRoutine(ImageHistoryFilter::LoadImageNotifyRoutine);
	if (NT_SUCCESS(*InitializeStatus) == FALSE)
	{
		DBGPRINT("ImageHistoryFilter!ImageHistoryFilter: Failed to register load image notify routine with status 0x%X.", *InitializeStatus);
		return;
	}

	FltInitializePushLock(&ImageHistoryFilter::ProcessHistoryLock);
}

/**
	Add a process to the linked-list of process history objects. This function attempts to add a history object regardless of failures.
	@param ProcessId - The process ID of the process to add.
	@param ParentId - The parent process ID of the process to add.
*/
VOID
ImageHistoryFilter::AddProcessToHistory (
	_In_ HANDLE ProcessId,
	_In_ HANDLE ParentId
	)
{
	NTSTATUS status;
	PPROCESS_HISTORY_ENTRY newProcessHistory;
	LARGE_INTEGER systemTime;
	LARGE_INTEGER localSystemTime;
	STACK_RETURN_INFO tempStackReturns[MAX_STACK_RETURN_HISTORY];
	BOOLEAN processHistoryLockHeld;

	processHistoryLockHeld = FALSE;

	if (ImageHistoryFilter::destroying)
	{
		return;
	}

	newProcessHistory = RCAST<PPROCESS_HISTORY_ENTRY>(ExAllocatePoolWithTag(PagedPool, sizeof(PROCESS_HISTORY_ENTRY), PROCESS_HISTORY_TAG));
	if (newProcessHistory == NULL)
	{
		DBGPRINT("ImageHistoryFilter!GetProcessImageFileName: Failed to allocate space for the process history.");
		return;
	}

	//
	// Basic fields.
	//
	newProcessHistory->ProcessId = ProcessId;
	newProcessHistory->ParentId = ParentId;
	newProcessHistory->CallerId = PsGetCurrentProcessId();
	newProcessHistory->ProcessTerminated = FALSE;
	KeQuerySystemTime(&systemTime);
	ExSystemTimeToLocalTime(&systemTime, &localSystemTime);
	NT_ASSERT(RtlTimeToSecondsSince1970(&localSystemTime, &newProcessHistory->EpochExecutionTime)); // Who is using this garbage after 2105??
	FltInitializePushLock(&newProcessHistory->ImageLoadHistoryLock);

	//
	// Image file name fields.
	//

	//
	// The new process name is a requirement.
	//
	if (ImageHistoryFilter::GetProcessImageFileName(ProcessId, &newProcessHistory->ProcessImageFileName) == FALSE)
	{
		DBGPRINT("ImageHistoryFilter!GetProcessImageFileName: Failed to get the name of the new process.");
		return;
	}

	//
	// These fields are optional.
	//
	ImageHistoryFilter::GetProcessImageFileName(ParentId, &newProcessHistory->ParentImageFileName);
	if (PsGetCurrentProcessId() != ParentId)
	{
		ImageHistoryFilter::GetProcessImageFileName(PsGetCurrentProcessId(), &newProcessHistory->CallerImageFileName);
	}

	//
	// Grab the user-mode stack.
	//
	newProcessHistory->CallerStackHistorySize = walker.WalkAndResolveStack(tempStackReturns, MAX_STACK_RETURN_HISTORY);
	newProcessHistory->CallerStackHistory = RCAST<PSTACK_RETURN_INFO>(ExAllocatePoolWithTag(PagedPool, sizeof(STACK_RETURN_INFO) * newProcessHistory->CallerStackHistorySize, STACK_HISTORY_TAG));

	//
	// Grab a lock to add an entry.
	//
	FltAcquirePushLockExclusive(&ImageHistoryFilter::ProcessHistoryLock);

	//
	// Check if the list has been initialized.
	//
	if (ImageHistoryFilter::ProcessHistoryHead == NULL)
	{
		ImageHistoryFilter::ProcessHistoryHead = newProcessHistory;
		InitializeListHead(RCAST<PLIST_ENTRY>(ImageHistoryFilter::ProcessHistoryHead));
	}
	//
	// Otherwise, just append the element to the end of the list.
	//
	else
	{
		InsertTailList(RCAST<PLIST_ENTRY>(ImageHistoryFilter::ProcessHistoryHead), RCAST<PLIST_ENTRY>(newProcessHistory));
	}

	FltReleasePushLock(&ImageHistoryFilter::ProcessHistoryLock);
}

/**
	Set a process to terminated, still maintain the history.
	@param ProcessId - The process ID of the process being terminated.
*/
VOID
ImageHistoryFilter::TerminateProcessInHistory (
	_In_ HANDLE ProcessId
	)
{
	PPROCESS_HISTORY_ENTRY currentProcessHistory;

	//
	// Acquire a shared lock to iterate processes.
	//
	FltAcquirePushLockShared(&ImageHistoryFilter::ProcessHistoryLock);

	//
	// Iterate histories for a match.
	//
	if (ImageHistoryFilter::ProcessHistoryHead)
	{
		currentProcessHistory = ImageHistoryFilter::ProcessHistoryHead;
		do
		{
			//
			// Find the process history with the same PID and then set it to terminated.
			//
			if (currentProcessHistory->ProcessId == ProcessId)
			{
				currentProcessHistory->ProcessTerminated = TRUE;
				break;
			}
			currentProcessHistory = RCAST<PPROCESS_HISTORY_ENTRY>(currentProcessHistory->ListEntry.Flink);
		} while (currentProcessHistory && currentProcessHistory != ImageHistoryFilter::ProcessHistoryHead);
	}

	//
	// Release the lock.
	//
	FltReleasePushLock(&ImageHistoryFilter::ProcessHistoryLock);
}

/**
	Notify routine called on new process execution.
	@param ParentId - The parent's process ID.
	@param ProcessId - The new child's process ID.
	@param Create - Whether or not this process is being created or terminated.
*/
VOID
ImageHistoryFilter::CreateProcessNotifyRoutine (
	HANDLE ParentId,
	HANDLE ProcessId,
	BOOLEAN Create
	)
{
	//
	// If a new process is being created, add it to the history of processes.
	//
	if (Create)
	{
		ImageHistoryFilter::AddProcessToHistory(ProcessId, ParentId);
	}
	else
	{
		//
		// Set the process as "terminated".
		//
		ImageHistoryFilter::TerminateProcessInHistory(ProcessId);
	}
}

/**
	Retrieve the full image file name for a process.
	@param ProcessId - The process to get the name of.
	@param ProcessImageFileName - PUNICODE_STRING to fill with the image file name of the process.
*/
BOOLEAN
ImageHistoryFilter::GetProcessImageFileName (
	_In_ HANDLE ProcessId,
	_Inout_ PUNICODE_STRING* ImageFileName
	)
{
	NTSTATUS status;
	PEPROCESS processObject;
	HANDLE processHandle;
	ULONG returnLength;

	processHandle = NULL;
	*ImageFileName = NULL;
	returnLength = 0;

	//
	// Before we can open a handle to the process, we need its PEPROCESS object.
	//
	status = PsLookupProcessByProcessId(ProcessId, &processObject);
	if (NT_SUCCESS(status) == FALSE)
	{
		DBGPRINT("ImageHistoryFilter!GetProcessImageFileName: Failed to find process object with status 0x%X.", status);
		goto Exit;
	}

	//
	// Open a handle to the process.
	//
	status = ObOpenObjectByPointer(processObject, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, GENERIC_ALL, *PsProcessType, KernelMode, &processHandle);
	if (NT_SUCCESS(status) == FALSE)
	{
		DBGPRINT("ImageHistoryFilter!GetProcessImageFileName: Failed to open handle to process with status 0x%X.", status);
		goto Exit;
	}

	//
	// Query for the size of the UNICODE_STRING.
	//
	status = NtQueryInformationProcess(processHandle, ProcessImageFileName, NULL, 0, &returnLength);
	if (status != STATUS_INFO_LENGTH_MISMATCH && status != STATUS_BUFFER_TOO_SMALL && status != STATUS_BUFFER_OVERFLOW)
	{
		DBGPRINT("ImageHistoryFilter!GetProcessImageFileName: Failed to query size of process ImageFileName with status 0x%X.", status);
		goto Exit;
	}

	//
	// Allocate the necessary space.
	//
	*ImageFileName = RCAST<PUNICODE_STRING>(ExAllocatePoolWithTag(PagedPool, returnLength, IMAGE_NAME_TAG));
	if (*ImageFileName == NULL)
	{
		DBGPRINT("ImageHistoryFilter!GetProcessImageFileName: Failed to allocate space for process ImageFileName.");
		goto Exit;
	}

	//
	// Query the image file name.
	//
	status = NtQueryInformationProcess(processHandle, ProcessImageFileName, NULL, 0, &returnLength);
	if (NT_SUCCESS(status) == FALSE)
	{
		DBGPRINT("ImageHistoryFilter!GetProcessImageFileName: Failed to query process ImageFileName with status 0x%X.", status);
		goto Exit;
	}
Exit:
	if (processHandle)
	{
		ZwClose(processHandle);
	}
	if (NT_SUCCESS(status) == FALSE && *ImageFileName)
	{
		ExFreePoolWithTag(*ImageFileName, IMAGE_NAME_TAG);
		*ImageFileName = NULL;
	}
	return NT_SUCCESS(status);
}