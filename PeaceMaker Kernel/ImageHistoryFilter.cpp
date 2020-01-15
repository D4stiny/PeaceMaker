#include "ImageHistoryFilter.h"

StackWalker ImageHistoryFilter::walker;
PPROCESS_HISTORY_ENTRY ImageHistoryFilter::ProcessHistoryHead;
EX_PUSH_LOCK ImageHistoryFilter::ProcessHistoryLock;
BOOLEAN ImageHistoryFilter::destroying;

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

	ImageHistoryFilter::ProcessHistoryHead = RCAST<PPROCESS_HISTORY_ENTRY>(ExAllocatePoolWithTag(PagedPool, sizeof(PROCESS_HISTORY_ENTRY), PROCESS_HISTORY_TAG));
	if (ImageHistoryFilter::ProcessHistoryHead == NULL)
	{
		DBGPRINT("ImageHistoryFilter!ImageHistoryFilter: Failed to allocate the process history head.");
		*InitializeStatus = STATUS_NO_MEMORY;
		return;
	}
	memset(ImageHistoryFilter::ProcessHistoryHead, 0, sizeof(PROCESS_HISTORY_ENTRY));
	InitializeListHead(RCAST<PLIST_ENTRY>(ImageHistoryFilter::ProcessHistoryHead));
}

/**
	Clean up the process history linked-list.
*/
ImageHistoryFilter::~ImageHistoryFilter (
	VOID
	)
{
	PPROCESS_HISTORY_ENTRY currentProcessHistory;
	PIMAGE_LOAD_HISTORY_ENTRY currentImageEntry;

	//
	// Set destroying to TRUE so that no other threads can get a lock.
	//
	ImageHistoryFilter::destroying = TRUE;

	//
	// Remove the notify routines.
	//
	PsSetCreateProcessNotifyRoutine(ImageHistoryFilter::CreateProcessNotifyRoutine, TRUE);
	PsRemoveLoadImageNotifyRoutine(ImageHistoryFilter::LoadImageNotifyRoutine);

	//
	// Acquire an exclusive lock to push out other threads.
	//
	FltAcquirePushLockExclusive(&ImageHistoryFilter::ProcessHistoryLock);

	//
	// Release the lock.
	//
	FltReleasePushLock(&ImageHistoryFilter::ProcessHistoryLock);

	//
	// Delete the lock for the process history linked-list.
	//
	FltDeletePushLock(&ImageHistoryFilter::ProcessHistoryLock);

	//
	// Go through each process history and free it.
	//
	if (ImageHistoryFilter::ProcessHistoryHead)
	{
		while (IsListEmpty(RCAST<PLIST_ENTRY>(ImageHistoryFilter::ProcessHistoryHead)) == FALSE)
		{
			currentProcessHistory = RCAST<PPROCESS_HISTORY_ENTRY>(RemoveHeadList(RCAST<PLIST_ENTRY>(ImageHistoryFilter::ProcessHistoryHead)));
			//
			// Clear the images linked-list.
			//
			FltDeletePushLock(&currentProcessHistory->ImageLoadHistoryLock);
			if (currentProcessHistory->ImageLoadHistory)
			{
				while (IsListEmpty(RCAST<PLIST_ENTRY>(currentProcessHistory->ImageLoadHistory)) == FALSE)
				{
					currentImageEntry = RCAST<PIMAGE_LOAD_HISTORY_ENTRY>(RemoveHeadList(RCAST<PLIST_ENTRY>(currentProcessHistory->ImageLoadHistory)));

					//
					// Free the image name.
					//
					if (currentImageEntry->ImageFileName.Buffer)
					{
						ExFreePoolWithTag(currentImageEntry->ImageFileName.Buffer, IMAGE_NAME_TAG);
					}

					//
					// Free the stack history.
					//
					ExFreePoolWithTag(currentImageEntry->CallerStackHistory, STACK_HISTORY_TAG);

					ExFreePoolWithTag(currentImageEntry, IMAGE_HISTORY_TAG);
				}

				//
				// Finally, free the list head.
				//
				ExFreePoolWithTag(currentProcessHistory->ImageLoadHistory, IMAGE_HISTORY_TAG);
			}

			//
			// Free the names.
			//
			if (currentProcessHistory->ProcessImageFileName)
			{
				ExFreePoolWithTag(currentProcessHistory->ProcessImageFileName, IMAGE_NAME_TAG);
			}
			if (currentProcessHistory->CallerImageFileName)
			{
				ExFreePoolWithTag(currentProcessHistory->CallerImageFileName, IMAGE_NAME_TAG);
			}
			if (currentProcessHistory->ParentImageFileName)
			{
				ExFreePoolWithTag(currentProcessHistory->ParentImageFileName, IMAGE_NAME_TAG);
			}

			//
			// Free the stack history.
			//
			ExFreePoolWithTag(currentProcessHistory->CallerStackHistory, STACK_HISTORY_TAG);

			//
			// Free the process history.
			//
			ExFreePoolWithTag(currentProcessHistory, PROCESS_HISTORY_TAG);
		}

		//
		// Finally, free the list head.
		//
		ExFreePoolWithTag(ImageHistoryFilter::ProcessHistoryHead, PROCESS_HISTORY_TAG);
	}
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
	BOOLEAN processHistoryLockHeld;

	processHistoryLockHeld = FALSE;
	status = STATUS_SUCCESS;

	if (ImageHistoryFilter::destroying)
	{
		return;
	}

	newProcessHistory = RCAST<PPROCESS_HISTORY_ENTRY>(ExAllocatePoolWithTag(PagedPool, sizeof(PROCESS_HISTORY_ENTRY), PROCESS_HISTORY_TAG));
	if (newProcessHistory == NULL)
	{
		DBGPRINT("ImageHistoryFilter!AddProcessToHistory: Failed to allocate space for the process history.");
		status = STATUS_NO_MEMORY;
		goto Exit;
	}

	memset(newProcessHistory, 0, sizeof(PROCESS_HISTORY_ENTRY));

	//
	// Basic fields.
	//
	newProcessHistory->ProcessId = ProcessId;
	newProcessHistory->ParentId = ParentId;
	newProcessHistory->CallerId = PsGetCurrentProcessId();
	newProcessHistory->ProcessTerminated = FALSE;
	newProcessHistory->ImageLoadHistorySize = 0;
	KeQuerySystemTime(&systemTime);
	ExSystemTimeToLocalTime(&systemTime, &localSystemTime);
	newProcessHistory->EpochExecutionTime = localSystemTime.QuadPart / TICKSPERSEC - SECS_1601_TO_1970;
	//
	// Image file name fields.
	//

	//
	// The new process name is a requirement.
	//
	if (ImageHistoryFilter::GetProcessImageFileName(ProcessId, &newProcessHistory->ProcessImageFileName) == FALSE)
	{
		DBGPRINT("ImageHistoryFilter!AddProcessToHistory: Failed to get the name of the new process.");
		status = STATUS_NOT_FOUND;
		goto Exit;
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
	newProcessHistory->CallerStackHistorySize = MAX_STACK_RETURN_HISTORY; // Will be updated in the resolve function.
	walker.WalkAndResolveStack(&newProcessHistory->CallerStackHistory, &newProcessHistory->CallerStackHistorySize, STACK_HISTORY_TAG);
	if (newProcessHistory->CallerStackHistory == NULL)
	{
		DBGPRINT("ImageHistoryFilter!AddProcessToHistory: Failed to allocate space for the stack history.");
		status = STATUS_NO_MEMORY;
		goto Exit;
	}

	newProcessHistory->ImageLoadHistory = RCAST<PIMAGE_LOAD_HISTORY_ENTRY>(ExAllocatePoolWithTag(PagedPool, sizeof(IMAGE_LOAD_HISTORY_ENTRY), IMAGE_HISTORY_TAG));
	if (newProcessHistory->ImageLoadHistory == NULL)
	{
		DBGPRINT("ImageHistoryFilter!AddProcessToHistory: Failed to allocate space for the image load history.");
		status = STATUS_NO_MEMORY;
		goto Exit;
	}
	memset(newProcessHistory->ImageLoadHistory, 0, sizeof(IMAGE_LOAD_HISTORY_ENTRY));
	InitializeListHead(RCAST<PLIST_ENTRY>(newProcessHistory->ImageLoadHistory));

	//
	// Initialize this last so we don't have to delete it if anything failed.
	//
	FltInitializePushLock(&newProcessHistory->ImageLoadHistoryLock);

	//
	// Grab a lock to add an entry.
	//
	FltAcquirePushLockExclusive(&ImageHistoryFilter::ProcessHistoryLock);

	InsertTailList(RCAST<PLIST_ENTRY>(ImageHistoryFilter::ProcessHistoryHead), RCAST<PLIST_ENTRY>(newProcessHistory));

	FltReleasePushLock(&ImageHistoryFilter::ProcessHistoryLock);
Exit:
	if (newProcessHistory && NT_SUCCESS(status) == FALSE)
	{
		ExFreePoolWithTag(newProcessHistory, PROCESS_HISTORY_TAG);
	}
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

	if (ImageHistoryFilter::destroying)
	{
		return;
	}

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
			currentProcessHistory = RCAST<PPROCESS_HISTORY_ENTRY>(currentProcessHistory->ListEntry.Blink);
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
	_In_ HANDLE ParentId,
	_In_ HANDLE ProcessId,
	_In_ BOOLEAN Create
	)
{
	//
	// If a new process is being created, add it to the history of processes.
	//
	if (Create)
	{
		ImageHistoryFilter::AddProcessToHistory(ProcessId, ParentId);
		DBGPRINT("ImageHistoryFilter!CreateProcessNotifyRoutine: Registered process 0x%X.", ProcessId);
	}
	else
	{
		DBGPRINT("ImageHistoryFilter!CreateProcessNotifyRoutine: Terminating process 0x%X.", ProcessId);
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
	status = NtQueryInformationProcess(processHandle, ProcessImageFileName, *ImageFileName, returnLength, &returnLength);
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

/**
	Notify routine called when a new image is loaded into a process. Adds the image to the corresponding process history element.
	@param FullImageName - A PUNICODE_STRING that identifies the executable image file. Might be NULL.
	@param ProcessId - The process ID where this image is being mapped.
	@param ImageInfo - Structure containing a variety of properties about the image being loaded.
*/
VOID
ImageHistoryFilter::LoadImageNotifyRoutine(
	_In_ PUNICODE_STRING FullImageName,
	_In_ HANDLE ProcessId,
	_In_ PIMAGE_INFO ImageInfo
	)
{
	NTSTATUS status;
	PPROCESS_HISTORY_ENTRY currentProcessHistory;
	PIMAGE_LOAD_HISTORY_ENTRY newImageLoadHistory;

	UNREFERENCED_PARAMETER(ImageInfo);

	currentProcessHistory = NULL;
	newImageLoadHistory = NULL;
	status = STATUS_SUCCESS;

	if (ImageHistoryFilter::destroying)
	{
		return;
	}

	DBGPRINT("ImageHistoryFilter!LoadImageNotifyRoutine(0x%X): Registering image %wZ.", ProcessId, FullImageName);

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
			if (currentProcessHistory->ProcessId == ProcessId && currentProcessHistory->ProcessTerminated == FALSE)
			{
				break;
			}
			currentProcessHistory = RCAST<PPROCESS_HISTORY_ENTRY>(currentProcessHistory->ListEntry.Blink);
		} while (currentProcessHistory && currentProcessHistory != ImageHistoryFilter::ProcessHistoryHead);
	}

	//
	// This might happen if we load on a running machine that already has processes.
	//
	if (currentProcessHistory == NULL || currentProcessHistory == ImageHistoryFilter::ProcessHistoryHead)
	{
		DBGPRINT("ImageHistoryFilter!LoadImageNotifyRoutine: Failed to find PID 0x%X in history.", ProcessId);
		status = STATUS_NOT_FOUND;
		goto Exit;
	}

	//
	// Allocate space for the new image history entry.
	//
	newImageLoadHistory = RCAST<PIMAGE_LOAD_HISTORY_ENTRY>(ExAllocatePoolWithTag(PagedPool, sizeof(IMAGE_LOAD_HISTORY_ENTRY), IMAGE_HISTORY_TAG));
	if (newImageLoadHistory == NULL)
	{
		DBGPRINT("ImageHistoryFilter!LoadImageNotifyRoutine: Failed to allocate space for the image history entry.");
		status = STATUS_NO_MEMORY;
		goto Exit;
	}
	memset(newImageLoadHistory, 0, sizeof(IMAGE_LOAD_HISTORY_ENTRY));

	//
	// Copy the image file name if it is provided.
	//
	if (FullImageName)
	{
		//
		// Allocate the copy buffer. FullImageName will not be valid forever.
		//
		newImageLoadHistory->ImageFileName.Buffer = RCAST<PWCH>(ExAllocatePoolWithTag(PagedPool, SCAST<SIZE_T>(FullImageName->Length) + 2, IMAGE_NAME_TAG));
		if (newImageLoadHistory->ImageFileName.Buffer == NULL)
		{
			DBGPRINT("ImageHistoryFilter!LoadImageNotifyRoutine: Failed to allocate space for the image file name.");
			status = STATUS_NO_MEMORY;
			goto Exit;
		}

		newImageLoadHistory->ImageFileName.Length = SCAST<SIZE_T>(FullImageName->Length) + 2;
		newImageLoadHistory->ImageFileName.MaximumLength = SCAST<SIZE_T>(FullImageName->Length) + 2;

		//
		// Copy the image name.
		//
		status = RtlStringCbCopyUnicodeString(newImageLoadHistory->ImageFileName.Buffer, SCAST<SIZE_T>(FullImageName->Length) + 2, FullImageName);
		if (NT_SUCCESS(status) == FALSE)
		{
			DBGPRINT("ImageHistoryFilter!LoadImageNotifyRoutine: Failed to copy the image file name with status 0x%X. Destination size = 0x%X, Source Size = 0x%X.", status, SCAST<SIZE_T>(FullImageName->Length) + 2, SCAST<SIZE_T>(FullImageName->Length));
			goto Exit;
		}
	}

	//
	// Grab the user-mode stack.
	//
	newImageLoadHistory->CallerStackHistorySize = MAX_STACK_RETURN_HISTORY; // Will be updated in the resolve function.
	walker.WalkAndResolveStack(&newImageLoadHistory->CallerStackHistory, &newImageLoadHistory->CallerStackHistorySize, STACK_HISTORY_TAG);
	if (newImageLoadHistory->CallerStackHistory == NULL)
	{
		DBGPRINT("ImageHistoryFilter!LoadImageNotifyRoutine: Failed to allocate space for the stack history.");
		status = STATUS_NO_MEMORY;
		goto Exit;
	}

	FltAcquirePushLockExclusive(&currentProcessHistory->ImageLoadHistoryLock);

	InsertHeadList(RCAST<PLIST_ENTRY>(currentProcessHistory->ImageLoadHistory), RCAST<PLIST_ENTRY>(newImageLoadHistory));
	currentProcessHistory->ImageLoadHistorySize++;

	FltReleasePushLock(&currentProcessHistory->ImageLoadHistoryLock);
Exit:
	//
	// Release the lock.
	//
	FltReleasePushLock(&ImageHistoryFilter::ProcessHistoryLock);

	//
	// Clean up on failure.
	//
	if (newImageLoadHistory && NT_SUCCESS(status) == FALSE)
	{
		if (newImageLoadHistory->ImageFileName.Buffer)
		{
			ExFreePoolWithTag(newImageLoadHistory->ImageFileName.Buffer, IMAGE_NAME_TAG);
			DBGPRINT("Free'd 'PmIn' at 0x%llx.", newImageLoadHistory->ImageFileName.Buffer);
		}
		if (newImageLoadHistory->CallerStackHistory)
		{
			ExFreePoolWithTag(newImageLoadHistory->CallerStackHistory, STACK_HISTORY_TAG);
			DBGPRINT("Free'd 'PmSh' at 0x%llx.", newImageLoadHistory->CallerStackHistory);
		}
		ExFreePoolWithTag(newImageLoadHistory, IMAGE_HISTORY_TAG);
		DBGPRINT("Free'd 'PmIh' at 0x%llx.", newImageLoadHistory);
	}
}

/**
	Get the summary for MaxProcessSummaries processes starting from the top of list + SkipCount.
	@param SkipCount - How many processes to skip in the list.
	@param ProcessSummaries - Caller-supplied array of process summaries that this function fills.
	@param MaxProcessSumaries - Maximum number of process summaries that the array allows for.
	@return The actual number of summaries returned.
*/
ULONG
ImageHistoryFilter::GetProcessHistorySummary (
	_In_ ULONG SkipCount,
	_Inout_ PPROCESS_SUMMARY_ENTRY ProcessSummaries,
	_In_ ULONG MaxProcessSummaries
	)
{
	PPROCESS_HISTORY_ENTRY currentProcessHistory;
	ULONG currentProcessIndex;
	ULONG actualFilledSummaries;
	NTSTATUS status;

	currentProcessIndex = 0;
	actualFilledSummaries = 0;

	if (ImageHistoryFilter::destroying)
	{
		return 0;
	}

	//
	// Acquire a shared lock to iterate processes.
	//
	FltAcquirePushLockShared(&ImageHistoryFilter::ProcessHistoryLock);

	//
	// Iterate histories for the MaxProcessSummaries processes after SkipCount processes.
	//
	if (ImageHistoryFilter::ProcessHistoryHead)
	{
		currentProcessHistory = RCAST<PPROCESS_HISTORY_ENTRY>(ImageHistoryFilter::ProcessHistoryHead->ListEntry.Blink);
		while (currentProcessHistory && currentProcessHistory != ImageHistoryFilter::ProcessHistoryHead && MaxProcessSummaries > actualFilledSummaries)
		{
			if (currentProcessIndex >= SkipCount)
			{
				DBGPRINT("ImageHistoryFilter!GetProcessHistorySummary: Adding process 0x%X.", currentProcessHistory->ProcessId);
				//
				// Fill out the summary.
				//
				ProcessSummaries[actualFilledSummaries].EpochExecutionTime = currentProcessHistory->EpochExecutionTime;
				ProcessSummaries[actualFilledSummaries].ProcessId = currentProcessHistory->ProcessId;
				ProcessSummaries[actualFilledSummaries].ProcessTerminated = currentProcessHistory->ProcessTerminated;
				
				if (currentProcessHistory->ProcessImageFileName)
				{
					//
					// Copy the image name.
					//
					status = RtlStringCbCopyUnicodeString(RCAST<NTSTRSAFE_PWSTR>(&ProcessSummaries[currentProcessIndex].ImageFileName), MAX_PATH * sizeof(WCHAR), currentProcessHistory->ProcessImageFileName);
					if (NT_SUCCESS(status) == FALSE)
					{
						DBGPRINT("ImageHistoryFilter!GetProcessHistorySummary: Failed to copy the image file name with status 0x%X.", status);
						break;
					}
				}
				actualFilledSummaries++;
			}
			currentProcessIndex++;
			currentProcessHistory = RCAST<PPROCESS_HISTORY_ENTRY>(currentProcessHistory->ListEntry.Blink);
		}
	}

	//
	// Release the lock.
	//
	FltReleasePushLock(&ImageHistoryFilter::ProcessHistoryLock);

	return actualFilledSummaries;
}

/**
	Populate a request for detailed information on a process.
	@param ProcessDetailedRequest - The request to populate.
*/
VOID
ImageHistoryFilter::PopulateProcessDetailedRequest (
	_Inout_ PPROCESS_DETAILED_REQUEST ProcessDetailedRequest
	)
{
	NTSTATUS status;
	PPROCESS_HISTORY_ENTRY currentProcessHistory;
	PIMAGE_LOAD_HISTORY_ENTRY currentImageEntry;
	ULONG i;

	i = 0;

	if (ImageHistoryFilter::destroying)
	{
		return;
	}

	//
	// Acquire a shared lock to iterate processes.
	//
	FltAcquirePushLockShared(&ImageHistoryFilter::ProcessHistoryLock);

	if (ImageHistoryFilter::ProcessHistoryHead)
	{
		currentProcessHistory = RCAST<PPROCESS_HISTORY_ENTRY>(ImageHistoryFilter::ProcessHistoryHead->ListEntry.Blink);
		while (currentProcessHistory && currentProcessHistory != ImageHistoryFilter::ProcessHistoryHead)
		{
			if (ProcessDetailedRequest->ProcessId == currentProcessHistory->ProcessId &&
				ProcessDetailedRequest->EpochExecutionTime == currentProcessHistory->EpochExecutionTime)
			{
				//
				// Set basic fields.
				//
				ProcessDetailedRequest->Populated = TRUE;
				ProcessDetailedRequest->CallerProcessId = currentProcessHistory->CallerId;
				ProcessDetailedRequest->ParentProcessId = currentProcessHistory->ParentId;

				//
				// Copy the stack history.
				//
				ProcessDetailedRequest->StackHistorySize = (ProcessDetailedRequest->StackHistorySize > currentProcessHistory->CallerStackHistorySize) ? currentProcessHistory->CallerStackHistorySize : ProcessDetailedRequest->StackHistorySize;
				memcpy(ProcessDetailedRequest->StackHistory, currentProcessHistory->CallerStackHistory, ProcessDetailedRequest->StackHistorySize * sizeof(STACK_RETURN_INFO));

				//
				// Copy the paths.
				//
				if (currentProcessHistory->ProcessImageFileName)
				{
					status = RtlStringCbCopyUnicodeString(RCAST<NTSTRSAFE_PWSTR>(ProcessDetailedRequest->ProcessPath), MAX_PATH * sizeof(WCHAR), currentProcessHistory->ProcessImageFileName);
					if (NT_SUCCESS(status) == FALSE)
					{
						DBGPRINT("ImageHistoryFilter!PopulateProcessDetailedRequest: Failed to copy the image file name of the process with status 0x%X.", status);
						break;
					}
				}
				if (currentProcessHistory->CallerImageFileName)
				{
					status = RtlStringCbCopyUnicodeString(RCAST<NTSTRSAFE_PWSTR>(ProcessDetailedRequest->CallerProcessPath), MAX_PATH * sizeof(WCHAR), currentProcessHistory->CallerImageFileName);
					if (NT_SUCCESS(status) == FALSE)
					{
						DBGPRINT("ImageHistoryFilter!PopulateProcessDetailedRequest: Failed to copy the image file name of the caller with status 0x%X.", status);
						break;
					}
				}
				if (currentProcessHistory->ParentImageFileName)
				{
					status = RtlStringCbCopyUnicodeString(RCAST<NTSTRSAFE_PWSTR>(ProcessDetailedRequest->ParentProcessPath), MAX_PATH * sizeof(WCHAR), currentProcessHistory->ParentImageFileName);
					if (NT_SUCCESS(status) == FALSE)
					{
						DBGPRINT("ImageHistoryFilter!PopulateProcessDetailedRequest: Failed to copy the image file name of the parent with status 0x%X.", status);
						break;
					}
				}

				//
				// Iterate the images for basic information.
				//
				FltAcquirePushLockShared(&currentProcessHistory->ImageLoadHistoryLock);

				//
				// The head isn't an element so skip it.
				//
				currentImageEntry = RCAST<PIMAGE_LOAD_HISTORY_ENTRY>(currentProcessHistory->ImageLoadHistory->ListEntry.Flink);
				while (currentImageEntry != currentProcessHistory->ImageLoadHistory && i < ProcessDetailedRequest->ImageSummarySize)
				{
					__try
					{
						if (currentImageEntry->ImageFileName.Buffer)
						{
							status = RtlStringCbCopyUnicodeString(RCAST<NTSTRSAFE_PWSTR>(ProcessDetailedRequest->ImageSummary[i].ImagePath), MAX_PATH * sizeof(WCHAR), &currentImageEntry->ImageFileName);
							if (NT_SUCCESS(status) == FALSE)
							{
								DBGPRINT("ImageHistoryFilter!PopulateProcessDetailedRequest: Failed to copy the image file name of an image with status 0x%X and source size %i.", status, currentImageEntry->ImageFileName.Length);
								break;
							}
						}
						ProcessDetailedRequest->ImageSummary[i].StackSize = currentImageEntry->CallerStackHistorySize;
					}
					__except (1)
					{
						DBGPRINT("ImageHistoryFilter!PopulateProcessDetailedRequest: Exception while processing image summaries.");
						break;
					}
					
					i++;

					currentImageEntry = RCAST<PIMAGE_LOAD_HISTORY_ENTRY>(currentImageEntry->ListEntry.Flink);
				}

				FltReleasePushLock(&currentProcessHistory->ImageLoadHistoryLock);

				ProcessDetailedRequest->ImageSummarySize = i; // Actual number of images put into the array.
				break;
			}
			currentProcessHistory = RCAST<PPROCESS_HISTORY_ENTRY>(currentProcessHistory->ListEntry.Blink);
		}
	}

	//
	// Release the lock.
	//
	FltReleasePushLock(&ImageHistoryFilter::ProcessHistoryLock);
}

/**
	Populate a process sizes request.
	@param ProcesSizesRequest - The request to populate.
*/
VOID
ImageHistoryFilter::PopulateProcessSizes (
	_Inout_ PPROCESS_SIZES_REQUEST ProcessSizesRequest
	)
{
	PPROCESS_HISTORY_ENTRY currentProcessHistory;

	if (ImageHistoryFilter::destroying)
	{
		return;
	}

	//
	// Acquire a shared lock to iterate processes.
	//
	FltAcquirePushLockShared(&ImageHistoryFilter::ProcessHistoryLock);

	if (ImageHistoryFilter::ProcessHistoryHead)
	{
		currentProcessHistory = RCAST<PPROCESS_HISTORY_ENTRY>(ImageHistoryFilter::ProcessHistoryHead->ListEntry.Blink);
		while (currentProcessHistory && currentProcessHistory != ImageHistoryFilter::ProcessHistoryHead)
		{
			if (ProcessSizesRequest->ProcessId == currentProcessHistory->ProcessId &&
				ProcessSizesRequest->EpochExecutionTime == currentProcessHistory->EpochExecutionTime)
			{
				ProcessSizesRequest->StackSize = currentProcessHistory->CallerStackHistorySize;
				ProcessSizesRequest->ImageSize = currentProcessHistory->ImageLoadHistorySize;
				break;
			}
			currentProcessHistory = RCAST<PPROCESS_HISTORY_ENTRY>(currentProcessHistory->ListEntry.Blink);
		}
	}

	//
	// Release the lock.
	//
	FltReleasePushLock(&ImageHistoryFilter::ProcessHistoryLock);
}

VOID
ImageHistoryFilter::PopulateImageDetailedRequest(
	_Inout_ PIMAGE_DETAILED_REQUEST ImageDetailedRequest
	)
{
	NTSTATUS status;
	PPROCESS_HISTORY_ENTRY currentProcessHistory;
	PIMAGE_LOAD_HISTORY_ENTRY currentImageEntry;
	ULONG i;

	i = 0;

	if (ImageHistoryFilter::destroying)
	{
		return;
	}

	//
	// Acquire a shared lock to iterate processes.
	//
	FltAcquirePushLockShared(&ImageHistoryFilter::ProcessHistoryLock);

	if (ImageHistoryFilter::ProcessHistoryHead)
	{
		currentProcessHistory = RCAST<PPROCESS_HISTORY_ENTRY>(ImageHistoryFilter::ProcessHistoryHead->ListEntry.Blink);
		while (currentProcessHistory && currentProcessHistory != ImageHistoryFilter::ProcessHistoryHead)
		{
			if (ImageDetailedRequest->ProcessId == currentProcessHistory->ProcessId &&
				ImageDetailedRequest->EpochExecutionTime == currentProcessHistory->EpochExecutionTime)
			{
				//
				// Iterate the images for basic information.
				//
				FltAcquirePushLockShared(&currentProcessHistory->ImageLoadHistoryLock);

				//
				// The head isn't an element so skip it.
				//
				currentImageEntry = RCAST<PIMAGE_LOAD_HISTORY_ENTRY>(currentProcessHistory->ImageLoadHistory->ListEntry.Flink);
				while (currentImageEntry != currentProcessHistory->ImageLoadHistory)
				{
					if (i == ImageDetailedRequest->ImageIndex)
					{
						if (currentImageEntry->ImageFileName.Buffer)
						{
							status = RtlStringCbCopyUnicodeString(RCAST<NTSTRSAFE_PWSTR>(ImageDetailedRequest->ImagePath), MAX_PATH * sizeof(WCHAR), &currentImageEntry->ImageFileName);
							if (NT_SUCCESS(status) == FALSE)
							{
								DBGPRINT("ImageHistoryFilter!PopulateImageDetailedRequest: Failed to copy the image file name of an image with status 0x%X and source size %i.", status, currentImageEntry->ImageFileName.Length);
								break;
							}
						}

						//
						// Copy the stack history.
						//
						ImageDetailedRequest->StackHistorySize = (ImageDetailedRequest->StackHistorySize > currentImageEntry->CallerStackHistorySize) ? currentImageEntry->CallerStackHistorySize : ImageDetailedRequest->StackHistorySize;
						memcpy(ImageDetailedRequest->StackHistory, currentImageEntry->CallerStackHistory, ImageDetailedRequest->StackHistorySize * sizeof(STACK_RETURN_INFO));

						ImageDetailedRequest->Populated = TRUE;
					}
					i++;
					currentImageEntry = RCAST<PIMAGE_LOAD_HISTORY_ENTRY>(currentImageEntry->ListEntry.Flink);
				}

				FltReleasePushLock(&currentProcessHistory->ImageLoadHistoryLock);
				break;
			}
			currentProcessHistory = RCAST<PPROCESS_HISTORY_ENTRY>(currentProcessHistory->ListEntry.Blink);
		}
	}

	//
	// Release the lock.
	//
	FltReleasePushLock(&ImageHistoryFilter::ProcessHistoryLock);
}