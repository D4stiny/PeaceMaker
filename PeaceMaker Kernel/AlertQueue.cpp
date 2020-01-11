#include "AlertQueue.h"

/**
	Initialize basic members of the AlertQueue class.
*/
AlertQueue::AlertQueue()
{
	alertsLock = RCAST<PKSPIN_LOCK>(ExAllocatePool(NonPagedPoolNx, sizeof(KSPIN_LOCK)));
	KeInitializeSpinLock(alertsLock);
	InitializeListHead(RCAST<PLIST_ENTRY>(&alertsHead));
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

	//
	// Allocate space for the new alert and copy the details.
	//
	newAlert = RCAST<PBASE_ALERT_INFO>(ExAllocatePoolWithTag(PagedPool, AlertSize, ALERT_QUEUE_ENTRY_TAG));
	memcpy(newAlert, Alert, AlertSize);

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
	return RCAST<PBASE_ALERT_INFO>(ExInterlockedRemoveHeadList(RCAST<PLIST_ENTRY>(&this->alertsHead), this->alertsLock));
}