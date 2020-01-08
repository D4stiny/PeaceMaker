#pragma once
#include "common.h"
#include "StackWalker.h"

//
// Maximum amount of STACK_RETURN_INFO to have in the process execution stack return history.
//
#define MAX_STACK_RETURN_HISTORY 20

typedef struct ImageLoadHistoryEntry
{
	LIST_ENTRY ListEntry;					// The list entry to iterate multiple images in a process.
	UNICODE_STRING ImageFileName;			// The full image file name of loaded image.
	PSTACK_RETURN_INFO CallerStackHistory;	// A variable-length array of the stack that loaded the image.
	USHORT CallerStackHistorySize;			// The size of the variable-length stack history array.
} IMAGE_LOAD_HISTORY_ENTRY, *PIMAGE_LOAD_HISTORY_ENTRY;

typedef struct ProcessHistoryEntry
{
	HANDLE CallerId;							// The process id of the caller process.
	HANDLE ParentId;							// The process id of the alleged parent process.
	UNICODE_STRING ParentImageFileName;			// The image file name of the alleged parent process. ONLY POPULATED IF CallerId != ParentId.

	HANDLE ProcessId;							// The process id of the executed process.
	UNICODE_STRING ProcessImageFileName;		// The image file name of the executed process.
	
	ULONG EpochExecutionTime;					// Process execution time in seconds since 1970.

	PSTACK_RETURN_INFO CallerStackHistory;		// A variable-length array of the stack that started the process.
	USHORT CallerStackHistorySize;				// The size of the variable-length stack history array.

	PIMAGE_LOAD_HISTORY_ENTRY ImageLoadHistory;	// A linked-list of loaded images and their respective stack histories.
	EX_PUSH_LOCK ImageLoadHistoryLock;			// The lock protecting the linked-list of loaded images.
} PROCESS_HISTORY_ENTRY, *PROCESS_HISTORY_ENTRY;

class ImageHistoryFilter
{

	static VOID CreateProcessNotifyRoutine ( HANDLE ParentId,
											 HANDLE ProcessId,
											 BOOLEAN Create
											 );

	static VOID LoadImageNotifyRoutine ( PUNICODE_STRING FullImageName,
										 HANDLE ProcessId,
										 PIMAGE_INFO ImageInfo
										 );
	

public:
	ImageHistoryFilter();
	~ImageHistoryFilter();


};