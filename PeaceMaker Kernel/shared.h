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

typedef struct StackReturnInfo
{
	PVOID RawAddress;			// The raw return address.
	BOOLEAN MemoryInModule;		// Whether or not the address is in a loaded module.
	BOOLEAN ExecutableMemory;	// Whether or not the address is in executable memory.
	WCHAR BinaryPath[MAX_PATH];	// The path of the binary this return address specifies.
	ULONG64 BinaryOffset;		// The offset in the binary that the return address refers to.
} STACK_RETURN_INFO, * PSTACK_RETURN_INFO;

typedef enum AlertType
{
	StackViolation,

} ALERT_TYPE;

typedef enum DetectionSource
{
	ProcessCreate,
	ImageLoad,
	RegistryFilterMatch,
	FileFilterMatch,
	ThreadCreate
} DETECTION_SOURCE;

typedef struct BaseAlertInfo
{
	LIST_ENTRY Entry;
	DETECTION_SOURCE AlertSource;
	ALERT_TYPE AlertType;
} BASE_ALERT_INFO, *PBASE_ALERT_INFO;

typedef struct StackViolationAlert
{
	BASE_ALERT_INFO AlertInformation;	// Basic alert information.
	PVOID ViolatingAddress;				// The specific address that was detected out-of-bounds.
	ULONG StackHistorySize;				// The length of the StackHistory array.
	STACK_RETURN_INFO StackHistory[1];	// Variable-length array of stack history.
} STACK_VIOLATION_ALERT, *PSTACK_VIOLATION_ALERT;