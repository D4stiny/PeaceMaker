#include "AlertQueue.h"

/**
	Initialize basic members of the AlertQueue class.
*/
AlertQueue::AlertQueue()
{
	this->alertsLock = RCAST<PKSPIN_LOCK>(ExAllocatePoolWithTag(NonPagedPool, sizeof(KSPIN_LOCK), ALERT_LOCK_TAG));
	NT_ASSERT(this->alertsLock);
	this->destroying = FALSE;
	KeInitializeSpinLock(this->alertsLock);
	InitializeListHead(RCAST<PLIST_ENTRY>(&this->alertsHead));
}

/**
	Clear the queue of alerts.
*/
AlertQueue::~AlertQueue()
{
	PLIST_ENTRY currentEntry;
	KIRQL oldIRQL;

	//
	// Make sure no one is doing operations on the AlertQueue.
	//
	this->destroying = TRUE;

	KeAcquireSpinLock(this->alertsLock, &oldIRQL);
	KeReleaseSpinLock(this->alertsLock, oldIRQL);

	while (IsListEmpty(RCAST<PLIST_ENTRY>(&this->alertsHead)) == FALSE)
	{
		currentEntry = RemoveHeadList(RCAST<PLIST_ENTRY>(&this->alertsHead));
		//
		// Free the entry.
		//
		ExFreePoolWithTag(SCAST<PVOID>(currentEntry), ALERT_QUEUE_ENTRY_TAG);
	}

	ExFreePoolWithTag(this->alertsLock, ALERT_LOCK_TAG);
}

/**
	Push an alert to the queue.
	@param Alert - The alert to push.
	@return Whether or not pushing the alert was successful.
*/
VOID
AlertQueue::PushAlert (
	_In_ PBASE_ALERT_INFO Alert,
	_In_ ULONG AlertSize
	)
{
	PBASE_ALERT_INFO newAlert;

	if (this->destroying)
	{
		return;
	}

	//
	// Allocate space for the new alert and copy the details.
	//
	newAlert = RCAST<PBASE_ALERT_INFO>(ExAllocatePoolWithTag(NonPagedPool, AlertSize, ALERT_QUEUE_ENTRY_TAG));
	if (newAlert == NULL)
	{
		DBGPRINT("AlertQueue!PushAlert: Failed to allocate space for new alert.");
		return;
	}
	memset(newAlert, 0, AlertSize);
	memcpy(newAlert, Alert, AlertSize);
	newAlert->AlertSize = AlertSize;

	//
	// Queue the alert.
	//
	ExInterlockedInsertTailList(RCAST<PLIST_ENTRY>(&this->alertsHead), RCAST<PLIST_ENTRY>(newAlert), this->alertsLock);
}

/**
	Pop an alert from the queue of alerts. Follows FI-FO.
	@return The first in queued alert.
*/
PBASE_ALERT_INFO
AlertQueue::PopAlert (
	VOID
	)
{
	if (this->destroying)
	{
		return NULL;
	}
	return RCAST<PBASE_ALERT_INFO>(ExInterlockedRemoveHeadList(RCAST<PLIST_ENTRY>(&this->alertsHead), this->alertsLock));
}

/**
	Check if the queue of alerts is empty.
	@return Whether or not the alerts queue is empty.
*/
BOOLEAN
AlertQueue::IsQueueEmpty (
	VOID
	)
{
	BOOLEAN empty;
	KIRQL oldIrql;

	ExAcquireSpinLock(this->alertsLock, &oldIrql);
	empty = IsListEmpty(RCAST<PLIST_ENTRY>(&this->alertsHead));
	ExReleaseSpinLock(this->alertsLock, oldIrql);

	return empty;
}


/**
	Free a previously pop'd alert.
	@param Alert - The alert to free.
*/
VOID
AlertQueue::FreeAlert(
	_In_ PBASE_ALERT_INFO Alert
	)
{
	ExFreePoolWithTag(Alert, ALERT_QUEUE_ENTRY_TAG);
}