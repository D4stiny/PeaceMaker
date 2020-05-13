/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 * 
 * COPYRIGHT Bill Demirkapi 2020
 */
#include "DetectionLogic.h"

/**
	Initialize class members.
*/
DetectionLogic::DetectionLogic()
{
	alerts = new (NonPagedPool, ALERT_QUEUE_TAG) AlertQueue();
}

/**
	Deconstruct class members.
*/
DetectionLogic::~DetectionLogic()
{
	alerts->~AlertQueue();
	ExFreePoolWithTag(alerts, ALERT_QUEUE_TAG);
}

/**
	Get the alert queue for this detection logic instance.
	@return The Alert Queue.
*/
PALERT_QUEUE
DetectionLogic::GetAlertQueue (
	VOID
	)
{
	return this->alerts;
}

/**
	Audit a stack history for invalid code.
	@param DetectionSource - The filter we are checking the stack of.
	@param SourceProcessId - The source of the audit.
	@param SourcePath - The source path.
	@param TargetPath - The target path.
	@param StackHistory - A variable-length array of stack return history.
	@param StackHistorySize - Size of the StackHistory array.
*/
VOID
DetectionLogic::AuditUserStackWalk (
	_In_ DETECTION_SOURCE DetectionSource,
	_In_ HANDLE SourceProcessId,
	_In_ PUNICODE_STRING SourcePath,
	_In_ PUNICODE_STRING TargetPath,
	_In_ STACK_RETURN_INFO StackHistory[],
	_In_ ULONG StackHistorySize
	)
{
	ULONG i;
	BOOLEAN stackViolation;
	PVOID firstViolatingAddress;

	stackViolation = FALSE;
	firstViolatingAddress = NULL;

	//
	// Check if any of the stack returns are to unmapped code.
	//
	for (i = 0; i < StackHistorySize; i++)
	{
		if (StackHistory[i].MemoryInModule == FALSE &&
			StackHistory[i].ExecutableMemory &&
			StackHistory[i].RawAddress != 0x0 &&
			RCAST<ULONG64>(StackHistory[i].RawAddress) < MmUserProbeAddress)
		{
			DBGPRINT("DetectionLogic!AuditUserStackWalk: Alert pid 0x%X, Violate 0x%llx, Source %i", PsGetCurrentProcessId(), StackHistory[i].RawAddress, DetectionSource);
			stackViolation = TRUE;
			firstViolatingAddress = StackHistory[i].RawAddress;
			break;
		}
	}

	if (stackViolation == FALSE)
	{
		return;
	}

	//
	// Push the alert.
	//
	this->PushStackViolationAlert(DetectionSource, firstViolatingAddress, SourceProcessId, SourcePath, TargetPath, StackHistory, StackHistorySize);
}

/**
	Create and push a stack violation alert.
	@param DetectionSource - The filter we are checking the stack of.
	@param ViolatingAddress - If the origin of this alert is from an audit address operation, log the specific address.
	@param SourceProcessId - The source of the audit.
	@param SourcePath - The source path.
	@param TargetPath - The target path.
	@param StackHistory - A variable-length array of stack return history.
	@param StackHistorySize - Size of the StackHistory array.
*/
VOID
DetectionLogic::PushStackViolationAlert(
	_In_ DETECTION_SOURCE DetectionSource,
	_In_ PVOID ViolatingAddress,
	_In_ HANDLE SourceProcessId,
	_In_ PUNICODE_STRING SourcePath,
	_In_ PUNICODE_STRING TargetPath,
	_In_ STACK_RETURN_INFO StackHistory[],
	_In_ ULONG StackHistorySize
	)
{
	ULONG stackHistoryBytes;
	PSTACK_VIOLATION_ALERT stackViolationAlert;

	//
	// Calculate the size of the StackHistory array in bytes.
	//
	stackHistoryBytes = sizeof(STACK_RETURN_INFO) * (StackHistorySize-1);

	//
	// Allocate space for the alert depending on the size of StackHistory.
	//
	stackViolationAlert = RCAST<PSTACK_VIOLATION_ALERT>(ExAllocatePoolWithTag(PagedPool, sizeof(STACK_VIOLATION_ALERT) + stackHistoryBytes, STACK_VIOLATION_TAG));
	if (stackViolationAlert == NULL)
	{
		DBGPRINT("DetectionLogic!PushStackViolationAlert: Failed to allocate space for the alert.");
		return;
	}
	memset(stackViolationAlert, 0, sizeof(STACK_VIOLATION_ALERT) + stackHistoryBytes);

	//
	// Fill the fields of the alert.
	//
	stackViolationAlert->AlertInformation.AlertType = StackViolation;
	stackViolationAlert->AlertInformation.AlertSource = DetectionSource;
	stackViolationAlert->ViolatingAddress = ViolatingAddress;
	stackViolationAlert->AlertInformation.SourceId = SourceProcessId;

	if (SourcePath)
	{
		RtlStringCbCopyUnicodeString(stackViolationAlert->AlertInformation.SourcePath, MAX_PATH, SourcePath);
	}
	if (TargetPath)
	{
		RtlStringCbCopyUnicodeString(stackViolationAlert->AlertInformation.TargetPath, MAX_PATH, TargetPath);
	}

	stackViolationAlert->StackHistorySize = StackHistorySize;
	memcpy(&stackViolationAlert->StackHistory, StackHistory, sizeof(STACK_RETURN_INFO) * StackHistorySize);

	//
	// Push the alert.
	//
	this->alerts->PushAlert(RCAST<PBASE_ALERT_INFO>(stackViolationAlert), sizeof(STACK_VIOLATION_ALERT) + stackHistoryBytes);

	//
	// PushAlert copies the alert, so we can free our copy.
	//
	ExFreePoolWithTag(stackViolationAlert, STACK_VIOLATION_TAG);
}

/**
	Validate a user-mode pointer.
	@param DetectionSource - The filter we are checking the stack of.
	@param UserPtr - The pointer to check.
	@param SourceProcessId - The source of the audit.
	@param SourcePath - The source path.
	@param TargetPath - The target path.
	@param StackHistory - A variable-length array of stack return history.
	@param StackHistorySize - Size of the StackHistory array.
*/
VOID
DetectionLogic::AuditUserPointer (
	_In_ DETECTION_SOURCE DetectionSource,
	_In_ PVOID UserPtr,
	_In_ HANDLE SourceProcessId,
	_In_ PUNICODE_STRING SourcePath,
	_In_ PUNICODE_STRING TargetPath,
	_In_ STACK_RETURN_INFO StackHistory[],
	_In_ ULONG StackHistorySize
	)
{
	STACK_RETURN_INFO info;

	info.RawAddress = UserPtr;

	//
	// Resolve basic information about the module.
	//
	resolver.ResolveAddressModule(UserPtr, &info);

	//
	// If the user pointer isn't mapped, something's wrong.
	//
	if (info.MemoryInModule == FALSE &&
		info.ExecutableMemory &&
		info.RawAddress != 0x0 &&
		RCAST<ULONG64>(info.RawAddress) < MmUserProbeAddress)
	{
		this->PushStackViolationAlert(DetectionSource, UserPtr, SourceProcessId, SourcePath, TargetPath, StackHistory, StackHistorySize);
	}
}

/**
	Check if an operation is on a remote process. This is called by suspicious operation callbacks such as Thread Creation.
	@param DetectionSource - The filter we are checking the stack of.
	@param UserPtr - The pointer to check.
	@param SourceProcessId - The source of the audit.
	@param SourceProcessId - The target of the operation.
	@param SourcePath - The source path.
	@param TargetPath - The target path.
	@param StackHistory - A variable-length array of stack return history.
	@param StackHistorySize - Size of the StackHistory array.
*/
VOID
DetectionLogic::AuditCallerProcessId(
	_In_ DETECTION_SOURCE DetectionSource,
	_In_ HANDLE CallerProcessId,
	_In_ HANDLE TargetProcessId,
	_In_ PUNICODE_STRING SourcePath,
	_In_ PUNICODE_STRING TargetPath,
	_In_ STACK_RETURN_INFO StackHistory[],
	_In_ ULONG StackHistorySize
	)
{
	ULONG stackHistoryBytes;
	PREMOTE_OPERATION_ALERT remoteOperationAlert;

	//
	// If the operation is on the current process, no problems!
	//
	if (CallerProcessId == TargetProcessId)
	{
		return;
	}

	//
	// Calculate the size of the StackHistory array in bytes.
	//
	stackHistoryBytes = sizeof(STACK_RETURN_INFO) * (StackHistorySize - 1);

	//
	// Allocate space for the alert depending on the size of StackHistory.
	//
	remoteOperationAlert = RCAST<PREMOTE_OPERATION_ALERT>(ExAllocatePoolWithTag(PagedPool, sizeof(REMOTE_OPERATION_ALERT) + stackHistoryBytes, STACK_VIOLATION_TAG));
	if (remoteOperationAlert == NULL)
	{
		DBGPRINT("DetectionLogic!PushStackViolationAlert: Failed to allocate space for the alert.");
		return;
	}
	memset(remoteOperationAlert, 0, sizeof(REMOTE_OPERATION_ALERT) + stackHistoryBytes);

	//
	// Fill the fields of the alert.
	//
	switch (DetectionSource)
	{
	case ProcessCreate:
		remoteOperationAlert->AlertInformation.AlertType = ParentProcessIdSpoofing;
		break;
	case ThreadCreate:
		remoteOperationAlert->AlertInformation.AlertType = RemoteThreadCreation;
		break;
	}
	remoteOperationAlert->AlertInformation.AlertSource = DetectionSource;
	remoteOperationAlert->AlertInformation.SourceId = CallerProcessId;
	remoteOperationAlert->RemoteTargetId = TargetProcessId;

	if (SourcePath)
	{
		RtlStringCbCopyUnicodeString(remoteOperationAlert->AlertInformation.SourcePath, MAX_PATH, SourcePath);
	}
	if (TargetPath)
	{
		RtlStringCbCopyUnicodeString(remoteOperationAlert->AlertInformation.TargetPath, MAX_PATH, TargetPath);
	}

	remoteOperationAlert->StackHistorySize = StackHistorySize;
	memcpy(&remoteOperationAlert->StackHistory, StackHistory, sizeof(STACK_RETURN_INFO) * StackHistorySize);

	//
	// Push the alert.
	//
	this->alerts->PushAlert(RCAST<PBASE_ALERT_INFO>(remoteOperationAlert), sizeof(REMOTE_OPERATION_ALERT) + stackHistoryBytes);

	//
	// PushAlert copies the alert, so we can free our copy.
	//
	ExFreePoolWithTag(remoteOperationAlert, STACK_VIOLATION_TAG);
}

/**
	Report a filter violation.
	@param DetectionSource - The filter type that was violated.
	@param CallerProcessId - The process ID of the caller that violated the filter.
	@param CallerPath - The path of the caller process.
	@param ViolatingPath - The path that triggered the filter violation.
	@param StackHistory - A variable-length array of stack return history.
	@param StackHistorySize - Size of the StackHistory array.
*/
VOID
DetectionLogic::ReportFilterViolation (
	_In_ DETECTION_SOURCE DetectionSource,
	_In_ HANDLE CallerProcessId,
	_In_ PUNICODE_STRING CallerPath,
	_In_ PUNICODE_STRING ViolatingPath,
	_In_ STACK_RETURN_INFO StackHistory[],
	_In_ ULONG StackHistorySize
	)
{
	ULONG stackHistoryBytes;
	PFILTER_VIOLATION_ALERT filterViolationAlert;

	//
	// Sanity check, sometimes stack history can be NULL if the stackwalk failed.
	//
	if (StackHistory == NULL || StackHistorySize == 0)
	{
		DBGPRINT("DetectionLogic!ReportFilterViolation: StackHistory was invalid!");
		return;
	}

	//
	// Calculate the size of the StackHistory array in bytes.
	//
	stackHistoryBytes = sizeof(STACK_RETURN_INFO) * (StackHistorySize - 1);

	//
	// Allocate space for the alert depending on the size of StackHistory.
	//
	filterViolationAlert = RCAST<PFILTER_VIOLATION_ALERT>(ExAllocatePoolWithTag(PagedPool, sizeof(FILTER_VIOLATION_ALERT) + stackHistoryBytes, STACK_VIOLATION_TAG));
	if (filterViolationAlert == NULL)
	{
		DBGPRINT("DetectionLogic!ReportFilterViolation: Failed to allocate space for the alert.");
		return;
	}
	memset(filterViolationAlert, 0, sizeof(FILTER_VIOLATION_ALERT) + stackHistoryBytes);

	filterViolationAlert->AlertInformation.AlertType = FilterViolation;
	filterViolationAlert->AlertInformation.AlertSource = DetectionSource;
	filterViolationAlert->AlertInformation.SourceId = CallerProcessId;

	if (CallerPath)
	{
		RtlStringCbCopyUnicodeString(filterViolationAlert->AlertInformation.SourcePath, MAX_PATH, CallerPath);
	}
	if (ViolatingPath)
	{
		RtlStringCbCopyUnicodeString(filterViolationAlert->AlertInformation.TargetPath, MAX_PATH, ViolatingPath);
	}

	filterViolationAlert->StackHistorySize = StackHistorySize;
	memcpy(&filterViolationAlert->StackHistory, StackHistory, sizeof(STACK_RETURN_INFO) * StackHistorySize);

	//
	// Push the alert.
	//
	this->alerts->PushAlert(RCAST<PBASE_ALERT_INFO>(filterViolationAlert), sizeof(FILTER_VIOLATION_ALERT) + stackHistoryBytes);

	//
	// PushAlert copies the alert, so we can free our copy.
	//
	ExFreePoolWithTag(filterViolationAlert, STACK_VIOLATION_TAG);
}