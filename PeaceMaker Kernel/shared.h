#pragma once
#if _KERNEL_MODE == 1
#include <fltKernel.h>
#else
#include <Windows.h>
#endif

#define IOCTL_ALERTS_QUEUED			CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0x1, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_POP_ALERT				CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0x2, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_GET_PROCESSES			CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0x3, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_GET_PROCESS_DETAILED	CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0x4, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_ADD_FILTER			CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0x5, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_LIST_FILTERS			CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0x6, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

//
// Maximum amount of STACK_RETURN_INFO to have in the process execution stack return history.
//
#define MAX_STACK_RETURN_HISTORY 30

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
	FilterViolation,
	ParentProcessIdSpoofing
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
	ULONG AlertSize;
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

//
// How many bytes the user-mode caller must supply as its output buffer.
//
#define MAX_STACK_VIOLATION_ALERT_SIZE sizeof(STACK_VIOLATION_ALERT) + (MAX_STACK_RETURN_HISTORY-1) * sizeof(STACK_RETURN_INFO)

typedef struct FilterViolationAlert
{
	BASE_ALERT_INFO AlertInformation;	// Basic alert information.
	ULONG StackHistorySize;				// The length of the StackHistory array.
	STACK_RETURN_INFO StackHistory[1];	// Variable-length array of stack history.
} FILTER_VIOLATION_ALERT, *PFILTER_VIOLATION_ALERT;

//
// How many bytes the user-mode caller must supply as its output buffer.
//
#define MAX_FILTER_VIOLATION_ALERT_SIZE sizeof(FILTER_VIOLATION_ALERT) + (MAX_STACK_RETURN_HISTORY-1) * sizeof(STACK_RETURN_INFO)

typedef struct ProcessSummaryRequest
{
	ULONG SkipCount;							// How many processes to skip.
	ULONG ProcessHistorySize;					// Size of the variable-length ProcessHistory array.
	PROCESS_SUMMARY_ENTRY ProcessHistory[1];	// Variable-length array of process history summaries.
} PROCESS_SUMMARY_REQUEST, *PPROCESS_SUMMARY_REQUEST;

#define MAX_PROCESS_SUMMARY_REQUEST_SIZE(summaryRequest) sizeof(PROCESS_SUMMARY_REQUEST) + (summaryRequest->ProcessHistorySize - 1) * sizeof(PROCESS_SUMMARY_ENTRY)

typedef struct ProcessDetailedRequest
{
	HANDLE ProcessId;					// The process id of the executed process.
	ULONG EpochExecutionTime;			// Process execution time in seconds since 1970.
	BOOLEAN Populated;					// Whether not this structure was populated (the process was found).

	WCHAR ProcessPath[MAX_PATH];		// The image file name of the executed process.

	HANDLE CallerProcessId;				// The process id of the caller process.
	WCHAR CallerProcessPath[MAX_PATH];	// OPTIONAL: The image file name of the caller process.

	HANDLE ParentProcessId;				// The process id of the alleged parent process.
	WCHAR ParentProcessPath[MAX_PATH];	// OPTIONAL: The image file name of the alleged parent process.

	ULONG StackHistorySize;				// The length of the StackHistory array.
	STACK_RETURN_INFO StackHistory[1];	// Variable-length array of stack history.
} PROCESS_DETAILED_REQUEST, *PPROCESS_DETAILED_REQUEST;

#define MAX_PROCESS_DETAILED_REQUEST_SIZE(detailedRequest) sizeof(PROCESS_DETAILED_REQUEST) + (detailedRequest->StackHistorySize - 1) * sizeof(STACK_RETURN_INFO)

typedef enum FilterType
{
	FilesystemFilter,
	RegistryFilter
} STRING_FILTER_TYPE, *PSTRING_FILTER_TYPE;

//
// Bitwise flags used for filtering for specific filters.
//
#define FILTER_FLAG_DELETE 0x1
#define FILTER_FLAG_WRITE 0x2
#define FILTER_FLAG_EXECUTE 0x4

#define FILTER_FLAG_ALL (FILTER_FLAG_DELETE | FILTER_FLAG_WRITE | FILTER_FLAG_EXECUTE)

typedef struct StringFilterRequest
{
	STRING_FILTER_TYPE FilterType;	// The general target of the filter (Filesystem/Registry).
	ULONG FilterId;					// Populated if filter successfully added.
	ULONG FilterFlags;				// Flags specifying what operations to filter (EXECUTE/WRITE/DELETE).
	ULONG FilterSize;				// The length of the filter.
	WCHAR FilterContent[MAX_PATH];	// The new filter to add.
} STRING_FILTER_REQUEST, *PSTRING_FILTER_REQUEST;

typedef struct FilterInfo
{
	ULONG Id;						// Unique ID of the filter used to remove entries.
	WCHAR MatchString[MAX_PATH];	// The string to match against. Always lowercase.
	ULONG Flags;					// Used by MatchesFilter to determine if should use filter. Caller specifies the filters they want via flag.
} FILTER_INFO, *PFILTER_INFO;

typedef struct ListFiltersRequest
{
	STRING_FILTER_TYPE FilterType;	// The general target of the filter (Filesystem/Registry).
	ULONG TotalFilters;				// Populated by the IOCTL request. The number of total filters there really are.
	ULONG SkipFilters;				// Number of filters to skip.
	ULONG CopiedFilters;			// Populated by the IOCTL request. Number of filters actually copied.
	FILTER_INFO Filters[10];		// You can request more than 10 filters via multiple requests.
} LIST_FILTERS_REQUEST, *PLIST_FILTERS_REQUEST;