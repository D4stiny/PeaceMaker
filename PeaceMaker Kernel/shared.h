#pragma once
#if _KERNEL_MODE == 1
#include <fltKernel.h>
#else
#include <Windows.h>
#endif

typedef struct ProcessSummaryEntry
{
	HANDLE ProcessId;				// The process id of the executed process.
	WCHAR ImageFileName[MAX_PATH];	// The image file name of the executed process.
	ULONG EpochExecutionTime;		// Process execution time in seconds since 1970.
	BOOLEAN ProcessTerminated;		// Whether or not the process has terminated.
} PROCESS_SUMMARY_ENTRY, * PPROCESS_SUMMARY_ENTRY;

typedef enum AlertType
{
	StackViolation,

} ALERT_TYPE;

typedef struct BaseAlertInfo
{
	LIST_ENTRY Entry;
	PCWCHAR AlertName;
	PCWCHAR AlertDescription;
	ALERT_TYPE AlertType;
} BASE_ALERT_INFO, *PBASE_ALERT_INFO;

typedef struct StackViolationAlert
{
	BASE_ALERT_INFO BasicAlertInformation;

};