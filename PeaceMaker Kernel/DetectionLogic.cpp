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