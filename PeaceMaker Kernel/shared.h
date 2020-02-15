/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 * 
 * COPYRIGHT Bill Demirkapi 2020
 */
#pragma once
#if _KERNEL_MODE == 1
#include <fltKernel.h>
#else
#include <Windows.h>
#endif

#define GLOBAL_NAME			L"\\\\.\\PeaceMaker"
#define NT_DEVICE_NAME      L"\\Device\\PeaceMaker"
#define DOS_DEVICE_NAME     L"\\DosDevices\\PeaceMaker"

#define IOCTL_ALERTS_QUEUED			CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0x1, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_POP_ALERT				CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0x2, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_GET_PROCESSES			CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0x3, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_GET_PROCESS_DETAILED	CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0x4, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_ADD_FILTER			CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0x5, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_LIST_FILTERS			CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0x6, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_GET_IMAGE_DETAILED	CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0x7, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_GET_PROCESS_SIZES		CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0x8, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_GET_GLOBAL_SIZES		CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0x9, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_DELETE_FILTER			CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0x10, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

#define RCAST reinterpret_cast
#define SCAST static_cast
#define CCAST const_cast

#define CONFIG_FILE_NAME "peacemaker.cfg"

//
// Maximum amount of STACK_RETURN_INFO to have in the process execution stack return history.
//
#define MAX_STACK_RETURN_HISTORY 30

typedef struct ProcessSummaryEntry
{
	HANDLE ProcessId;				// The process id of the executed process.
	WCHAR ImageFileName[MAX_PATH];	// The image file name of the executed process.
	ULONGLONG EpochExecutionTime;	// Process execution time in seconds since 1970.
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

typedef struct ProcessSummaryRequest
{
	ULONG SkipCount;							// How many processes to skip.
	ULONG ProcessHistorySize;					// Size of the variable-length ProcessHistory array.
	PROCESS_SUMMARY_ENTRY ProcessHistory[1];	// Variable-length array of process history summaries.
} PROCESS_SUMMARY_REQUEST, *PPROCESS_SUMMARY_REQUEST;

#define MAX_PROCESS_SUMMARY_REQUEST_SIZE_RAW(size) sizeof(PROCESS_SUMMARY_REQUEST) + (size - 1) * sizeof(PROCESS_SUMMARY_ENTRY)
#define MAX_PROCESS_SUMMARY_REQUEST_SIZE(summaryRequest) MAX_PROCESS_SUMMARY_REQUEST_SIZE_RAW(summaryRequest->ProcessHistorySize)

typedef struct ImageSummary
{
	WCHAR ImagePath[MAX_PATH];	// The path to the image. Populated by the driver.
	ULONG StackSize;			// The size of the stack history.
} IMAGE_SUMMARY, *PIMAGE_SUMMARY;

typedef struct ProcessDetailedRequest
{
	HANDLE ProcessId;					// The process id of the executed process.
	ULONGLONG EpochExecutionTime;		// Process execution time in seconds since 1970.
	BOOLEAN Populated;					// Whether not this structure was populated (the process was found).

	WCHAR ProcessPath[MAX_PATH];		// The image file name of the executed process.

	HANDLE CallerProcessId;				// The process id of the caller process.
	WCHAR CallerProcessPath[MAX_PATH];	// OPTIONAL: The image file name of the caller process.

	HANDLE ParentProcessId;				// The process id of the alleged parent process.
	WCHAR ParentProcessPath[MAX_PATH];	// OPTIONAL: The image file name of the alleged parent process.

	WCHAR ProcessCommandLine[MAX_PATH]; // The process command line.

	ULONG ImageSummarySize;				// The length of the ImageSummary array.
	PIMAGE_SUMMARY ImageSummary;		// Variable-length array of image summaries.

	ULONG StackHistorySize;				// The length of the StackHistory array.
	PSTACK_RETURN_INFO StackHistory;	// Variable-length array of stack history.
} PROCESS_DETAILED_REQUEST, *PPROCESS_DETAILED_REQUEST;

typedef struct ImageDetailedRequest
{
	HANDLE ProcessId;					// The process id of the executed process.
	ULONGLONG EpochExecutionTime;		// Process execution time in seconds since 1970.
	BOOLEAN Populated;					// Whether not this structure was populated (the image was found).

	ULONG ImageIndex;					// The index of the target image. Must not be larger than the process images list size.
	WCHAR ImagePath[MAX_PATH];			// The path to the image. Populated by the driver.
	ULONG StackHistorySize;				// The length of the StackHistory array.
	STACK_RETURN_INFO StackHistory[1];	// Variable-length array of stack history. Populated by the driver.
} IMAGE_DETAILED_REQUEST, *PIMAGE_DETAILED_REQUEST;

#define MAX_IMAGE_DETAILED_REQUEST_SIZE_RAW(size) sizeof(IMAGE_DETAILED_REQUEST) + ((size - 1) * sizeof(STACK_RETURN_INFO))
#define MAX_IMAGE_DETAILED_REQUEST_SIZE(detailedRequest) MAX_IMAGE_DETAILED_REQUEST_SIZE_RAW(detailedRequest->StackHistorySize)

typedef struct ProcessSizesRequest
{
	HANDLE ProcessId;					// The process id of the executed process.
	ULONGLONG EpochExecutionTime;		// Process execution time in seconds since 1970.
	ULONG ProcessSize;					// The number of loaded processes.
	ULONG ImageSize;					// The number of loaded images in the process.
	ULONG StackSize;					// The number of stack return entries in the stack history for the process.
} PROCESS_SIZES_REQUEST, *PPROCESS_SIZES_REQUEST;

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

#define FlagOn(_F,_SF) ((_F) & (_SF))

#define FILTER_FLAG_ALL (FILTER_FLAG_DELETE | FILTER_FLAG_WRITE | FILTER_FLAG_EXECUTE)

typedef struct FilterInfo
{
	ULONG Id;						// Unique ID of the filter used to remove entries.
	STRING_FILTER_TYPE Type;		// The general target of the filter (Filesystem/Registry).
	WCHAR MatchString[MAX_PATH];	// The string to match against. Always lowercase.
	ULONG MatchStringSize;			// The length of the match string.
	ULONG Flags;					// Used by MatchesFilter to determine if should use filter. Caller specifies the filters they want via flag.
} FILTER_INFO, *PFILTER_INFO;

typedef struct StringFilterRequest
{
	STRING_FILTER_TYPE FilterType;	// The general target of the filter (Filesystem/Registry).
	FILTER_INFO Filter;
} STRING_FILTER_REQUEST, * PSTRING_FILTER_REQUEST;

typedef struct ListFiltersRequest
{
	STRING_FILTER_TYPE FilterType;	// The general target of the filter (Filesystem/Registry).
	ULONG TotalFilters;				// Populated by the IOCTL request. The number of total filters there really are.
	ULONG SkipFilters;				// Number of filters to skip.
	ULONG CopiedFilters;			// Populated by the IOCTL request. Number of filters actually copied.
	FILTER_INFO Filters[10];		// You can request more than 10 filters via multiple requests.
} LIST_FILTERS_REQUEST, *PLIST_FILTERS_REQUEST;

typedef enum AlertType
{
	StackViolation,
	FilterViolation,
	ParentProcessIdSpoofing,
	RemoteThreadCreation
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
	LIST_ENTRY Entry;				// The LIST_ENTRY details.
	ULONG AlertSize;				// The size (in bytes) of the structure.
	DETECTION_SOURCE AlertSource;	// The source of the alert.
	ALERT_TYPE AlertType;			// The type of alert.
	HANDLE SourceId;				// The process id of the source of the alert.
	WCHAR SourcePath[MAX_PATH];		// The path to the source.
	WCHAR TargetPath[MAX_PATH];		// The path to the target.
} BASE_ALERT_INFO, * PBASE_ALERT_INFO;

typedef struct StackViolationAlert
{
	BASE_ALERT_INFO AlertInformation;	// Basic alert information.
	PVOID ViolatingAddress;				// The specific address that was detected out-of-bounds.
	ULONG StackHistorySize;				// The length of the StackHistory array.
	STACK_RETURN_INFO StackHistory[1];	// Variable-length array of stack history.
} STACK_VIOLATION_ALERT, * PSTACK_VIOLATION_ALERT;

//
// How many bytes the user-mode caller must supply as its output buffer.
//
#define MAX_STACK_VIOLATION_ALERT_SIZE sizeof(STACK_VIOLATION_ALERT) + (MAX_STACK_RETURN_HISTORY-1) * sizeof(STACK_RETURN_INFO)

typedef struct FilterViolationAlert
{
	BASE_ALERT_INFO AlertInformation;	// Basic alert information.
	ULONG StackHistorySize;				// The length of the StackHistory array.
	STACK_RETURN_INFO StackHistory[1];	// Variable-length array of stack history.
} FILTER_VIOLATION_ALERT, * PFILTER_VIOLATION_ALERT;

//
// How many bytes the user-mode caller must supply as its output buffer.
//
#define MAX_FILTER_VIOLATION_ALERT_SIZE sizeof(FILTER_VIOLATION_ALERT) + (MAX_STACK_RETURN_HISTORY-1) * sizeof(STACK_RETURN_INFO)

typedef struct RemoteOperationAlert
{
	BASE_ALERT_INFO AlertInformation;	// Basic alert information.
	HANDLE RemoteTargetId;				// Process ID of the target process.
	ULONG StackHistorySize;				// The length of the StackHistory array.
	STACK_RETURN_INFO StackHistory[1];	// Variable-length array of stack history.
} REMOTE_OPERATION_ALERT, *PREMOTE_OPERATION_ALERT;

//
// How many bytes the user-mode caller must supply as its output buffer.
//
#define MAX_REMOTE_OPERATION_ALERT_SIZE sizeof(REMOTE_OPERATION_ALERT) + (MAX_STACK_RETURN_HISTORY-1) * sizeof(STACK_RETURN_INFO)

typedef struct GlobalSizes
{
	ULONG64 ProcessHistorySize;
	ULONG64 FilesystemFilterSize;
	ULONG64 RegistryFilterSize;
} GLOBAL_SIZES, *PGLOBAL_SIZES;

typedef struct DeleteFilterRequest
{
	ULONG FilterId;					// Unique ID of the filter used to remove entries.
	STRING_FILTER_TYPE FilterType;	// The general target of the filter (Filesystem/Registry).
	BOOLEAN Deleted;				// Whether or not the filter was deleted.
} DELETE_FILTER_REQUEST, *PDELETE_FILTER_REQUEST;