/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 * 
 * COPYRIGHT Bill Demirkapi 2020
 */
#include "ThreadFilter.h"

PDETECTION_LOGIC ThreadFilter::Detector;	// Detection utility.
STACK_WALKER ThreadFilter::Walker;			// Stack walking utility.

/**
	Initialize thread notify filters to detect manually mapped threads.
	@param DetectionLogic - Detection instance used to analyze untrusted operations.
	@param InitializeStatus - Status of initialization.
*/
ThreadFilter::ThreadFilter (
	_In_ PDETECTION_LOGIC DetectionLogic,
	_Inout_ NTSTATUS* InitializeStatus
	)
{
	ThreadFilter::Detector = DetectionLogic;

	//
	// Create a thread notify routine.
	//
	*InitializeStatus = PsSetCreateThreadNotifyRoutine(ThreadFilter::ThreadNotifyRoutine);
	if (NT_SUCCESS(*InitializeStatus) == FALSE)
	{
		DBGPRINT("ThreadFilter!ThreadFilter: Failed to create thread notify routine with status 0x%X.", *InitializeStatus);
	}
}

/**
	Teardown dynamic components of the thread filter.
*/
ThreadFilter::~ThreadFilter (
	VOID
	)
{
	PsRemoveCreateThreadNotifyRoutine(ThreadFilter::ThreadNotifyRoutine);
}

/**
	Query a thread's start address for validation.
	@param ThreadId - The target thread's ID.
	@return The start address of the target thread.
*/
PVOID
ThreadFilter::GetThreadStartAddress (
	_In_ HANDLE ThreadId
	)
{
	NTSTATUS status;
	PVOID startAddress;
	PETHREAD threadObject;
	HANDLE threadHandle;
	ULONG returnLength;

	startAddress = NULL;
	threadHandle = NULL;

	//
	// First look up the PETHREAD of the thread.
	//
	status = PsLookupThreadByThreadId(ThreadId, &threadObject);
	if (NT_SUCCESS(status) == FALSE)
	{
		DBGPRINT("ThreadFilter!GetThreadStartAddress: Failed to lookup thread 0x%X by its ID.", ThreadId);
		goto Exit;
	}

	//
	// Open a handle to the thread.
	//
	status = ObOpenObjectByPointer(threadObject, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, GENERIC_ALL, *PsThreadType, KernelMode, &threadHandle);
	if (NT_SUCCESS(status) == FALSE)
	{
		DBGPRINT("ThreadFilter!GetThreadStartAddress: Failed to open handle to process with status 0x%X.", status);
		goto Exit;
	}

	//
	// Query the thread's start address.
	//
	status = NtQueryInformationThread(threadHandle, ThreadQuerySetWin32StartAddress, &startAddress, sizeof(startAddress), &returnLength);
	if (NT_SUCCESS(status) == FALSE)
	{
		DBGPRINT("ThreadFilter!GetThreadStartAddress: Failed to query thread start address with status 0x%X.", status);
		goto Exit;
	}
Exit:
	if (threadHandle != NULL)
	{
		ZwClose(threadHandle);
	}
	return startAddress;
}

/**
	Called when a new thread is created. Ensure the thread is legit.
	@param ProcessId - The process ID of the process receiving the new thread.
	@param ThreadId - The thread ID of the new thread.
	@param Create - Whether or not this is termination of a thread or creation.
*/
VOID
ThreadFilter::ThreadNotifyRoutine (
	_In_ HANDLE ProcessId,
	_In_ HANDLE ThreadId,
	_In_ BOOLEAN Create
	)
{
	ULONG processThreadCount;
	PVOID threadStartAddress;
	PSTACK_RETURN_INFO threadCreateStack;
	ULONG threadCreateStackSize;
	PUNICODE_STRING threadCallerName;
	PUNICODE_STRING threadTargetName;

	threadCreateStack = NULL;
	threadCreateStackSize = 20;
	threadCallerName = NULL;
	threadTargetName = NULL;

	//
	// We don't really care about thread termination or if the thread is kernel-mode.
	//
	if (Create == FALSE || ExGetPreviousMode() == KernelMode)
	{
		return;
	}

	//
	// If we can't find the process or it's the first thread of the process, skip it.
	//
	if (ImageHistoryFilter::AddProcessThreadCount(ProcessId, &processThreadCount) == FALSE ||
		processThreadCount <= 1)
	{
		return;
	}

	//
	// Walk the stack.
	//
	ThreadFilter::Walker.WalkAndResolveStack(&threadCreateStack, &threadCreateStackSize, STACK_HISTORY_TAG);

	//
	// Grab the name of the caller.
	//
	if (ImageHistoryFilter::GetProcessImageFileName(PsGetCurrentProcessId(), &threadCallerName) == FALSE)
	{
		goto Exit;
	}

	threadTargetName = threadCallerName;

	//
	// We only need to resolve again if the target process is a different than the caller.
	//
	if (PsGetCurrentProcessId() != ProcessId)
	{
		//
		// Grab the name of the target.
		//
		if (ImageHistoryFilter::GetProcessImageFileName(ProcessId, &threadTargetName) == FALSE)
		{
			goto Exit;
		}
	}
	
	//
	// Grab the start address of the thread.
	//
	threadStartAddress = ThreadFilter::GetThreadStartAddress(ThreadId);

	//
	// Audit the target's start address.
	//
	ThreadFilter::Detector->AuditUserPointer(ThreadCreate, threadStartAddress, PsGetCurrentProcessId(), threadCallerName, threadTargetName, threadCreateStack, threadCreateStackSize);

	//
	// Audit the caller's stack.
	//
	ThreadFilter::Detector->AuditUserStackWalk(ThreadCreate, PsGetCurrentProcessId(), threadCallerName, threadTargetName, threadCreateStack, threadCreateStackSize);

	//
	// Check if this is a remote operation.
	//
	ThreadFilter::Detector->AuditCallerProcessId(ThreadCreate, PsGetCurrentProcessId(), ProcessId, threadCallerName, threadTargetName, threadCreateStack, threadCreateStackSize);
Exit:
	if (threadCreateStack != NULL)
	{
		ExFreePoolWithTag(threadCreateStack, STACK_HISTORY_TAG);
	}
	if (threadCallerName != NULL)
	{
		ExFreePoolWithTag(threadCallerName, IMAGE_NAME_TAG);
	}
	if (threadCallerName != threadTargetName && threadTargetName != NULL)
	{
		ExFreePoolWithTag(threadTargetName, IMAGE_NAME_TAG);
	}
}