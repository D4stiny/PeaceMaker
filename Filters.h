#pragma once
#include "common.h"

#define FILTER_INFO_TAG 'iFmP'

typedef struct FILTER_INFO
{
	SINGLE_LIST_ENTRY SingleListEntry;	// The single list entry used to iterate multiple filters.
	ULONG Id;								// Unique ID of the filter used to remove entries.
	WCHAR MatchString[MAX_PATH];		// The string to match against. Always lowercase.
} FILTERINFO, *PFILTERINFO;

typedef class CUSTOM_FILTERS
{
	PFILTERINFO filtersHead; // The linked list of filters.
	EX_PUSH_LOCK filtersLock; // The lock protecting the linked list of filters.
	BOOLEAN destroying; // This boolean indicates to functions that a lock should not be held as we are in the process of destruction.

public:
	CUSTOM_FILTERS();
	VOID DestructCustomFilters();

	ULONG AddFilter(WCHAR* MatchString);
	BOOLEAN RemoveFilter(ULONG FilterId);

	BOOLEAN MatchesFilter(WCHAR* StrToCmp);
} *PCUSTOM_FILTERS;

