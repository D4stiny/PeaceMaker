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
	@param StackHistory - A variable-length array of stack return history.
	@param StackHistorySize - Size of the StackHistory array.
*/
VOID
DetectionLogic::AuditUserStackWalk (
	_In_ DETECTION_SOURCE DetectionSource,
	_In_ STACK_RETURN_INFO StackHistory[],
	_In_ ULONG StackHistorySize
	)
{
	ULONG i;
	BOOLEAN stackViolation;

	stackViolation = FALSE;

	//
	// Check if any of the stack returns are to unmapped code.
	//
	for (i = 0; i < StackHistorySize; i++)
	{
		if (StackHistory[i].MemoryInModule == FALSE)
		{
			stackViolation = TRUE;
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
	this->PushStackViolationAlert(DetectionSource, NULL, StackHistory, StackHistorySize);
}

/**
	Create and push a stack violation alert.
	@param DetectionSource - The filter we are checking the stack of.
	@param ViolatingAddress - If the origin of this alert is from an audit address operation, log the specific address.
	@param StackHistory - A variable-length array of stack return history.
	@param StackHistorySize - Size of the StackHistory array.
*/
VOID
DetectionLogic::PushStackViolationAlert(
	_In_ DETECTION_SOURCE DetectionSource,
	_In_ PVOID ViolatingAddress,
	_In_ STACK_RETURN_INFO StackHistory[],
	_In_ ULONG StackHistorySize
	)
{
	ULONG stackHistoryBytes;
	PSTACK_VIOLATION_ALERT stackViolationAlert;

	//
	// Calculate the size of the StackHistory array in bytes.
	//
	stackHistoryBytes = sizeof(STACK_RETURN_INFO) * StackHistorySize;

	//
	// Allocate space for the alert depending on the size of StackHistory.
	//
	stackViolationAlert = RCAST<PSTACK_VIOLATION_ALERT>(ExAllocatePoolWithTag(PagedPool, sizeof(STACK_VIOLATION_ALERT) + stackHistoryBytes, STACK_VIOLATION_TAG));
	if (stackViolationAlert == NULL)
	{
		DBGPRINT("DetectionLogic!PushStackViolationAlert: Failed to allocate space for the alert.");
		return;
	}

	//
	// Fill the fields of the alert.
	//
	stackViolationAlert->AlertInformation.AlertType = StackViolation;
	stackViolationAlert->AlertInformation.AlertSource = DetectionSource;
	stackViolationAlert->ViolatingAddress = ViolatingAddress;
	stackViolationAlert->StackHistorySize = StackHistorySize;
	memcpy(&stackViolationAlert->StackHistory, StackHistory, stackHistoryBytes);

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
	@param StackHistory - A variable-length array of stack return history.
	@param StackHistorySize - Size of the StackHistory array.
*/
VOID
DetectionLogic::AuditUserPointer (
	_In_ DETECTION_SOURCE DetectionSource,
	_In_ PVOID UserPtr,
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
	if (info.MemoryInModule == FALSE)
	{
		this->PushStackViolationAlert(DetectionSource, UserPtr, StackHistory, StackHistorySize);
	}
}