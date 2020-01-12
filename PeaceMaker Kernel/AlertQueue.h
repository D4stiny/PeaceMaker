#pragma once
#include "common.h"
#include "shared.h"

typedef class AlertQueue
{
	BASE_ALERT_INFO alertsHead; // The linked list of alerts.
	PKSPIN_LOCK alertsLock; // The lock protecting the linked-list of alerts.
	BOOLEAN destroying; // This boolean indicates to functions that a lock should not be held as we are in the process of destruction.

public:
	AlertQueue();
	~AlertQueue();

	VOID PushAlert (
		_In_ PBASE_ALERT_INFO Alert,
		_In_ ULONG AlertSize
		);

	PBASE_ALERT_INFO PopAlert (
		VOID
		);

	BOOLEAN IsQueueEmpty (
		VOID
		);

	VOID FreeAlert (
		_In_ PBASE_ALERT_INFO Alert
		);

} ALERT_QUEUE, *PALERT_QUEUE;

#define ALERT_LOCK_TAG 'lAmP'
#define ALERT_QUEUE_ENTRY_TAG 'eAmP'