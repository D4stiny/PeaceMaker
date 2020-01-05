#pragma once
#include "common.h"

#define FILTER_INFO_TAG 'iFmP'

//
// Bitwise flags used for filtering for specific filters.
//
#define FILTER_FLAG_DELETE 0x1
#define FILTER_FLAG_WRITE 0x2
#define FILTER_FLAG_EXECUTE 0x4

#define FILTER_FLAG_ALL (FILTER_FLAG_DELETE | FILTER_FLAG_WRITE | FILTER_FLAG_EXECUTE)

typedef struct FILTER_INFO
{
	LIST_ENTRY ListEntry;			// The list entry used to iterate multiple filters.
	ULONG Id;						// Unique ID of the filter used to remove entries.
	WCHAR MatchString[MAX_PATH];	// The string to match against. Always lowercase.
	ULONG Flags;					// Used by MatchesFilter to determine if should use filter. Caller specifies the filters they want via flag.
} FILTERINFO, *PFILTERINFO;

typedef class StringFilters
{
	PFILTERINFO filtersHead; // The linked list of filters.
	EX_PUSH_LOCK filtersLock; // The lock protecting the linked list of filters.
	BOOLEAN destroying; // This boolean indicates to functions that a lock should not be held as we are in the process of destruction.

public:
	StringFilters();
	~StringFilters();

	ULONG AddFilter(WCHAR* MatchString, ULONG OperationFlag);
	BOOLEAN RemoveFilter(ULONG FilterId);

	BOOLEAN MatchesFilter(WCHAR* StrToCmp, ULONG OperationFlag);
} STRING_FILTERS, *PSTRING_FILTERS;

