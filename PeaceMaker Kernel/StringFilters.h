#pragma once
#include "common.h"
#include "shared.h"

typedef struct FilterInfoLinked
{
	LIST_ENTRY ListEntry;	// The list entry used to iterate multiple filters.
	FILTER_INFO Filter;		// The filter itself.
} FILTER_INFO_LINKED, *PFILTER_INFO_LINKED;

typedef class StringFilters
{
	PFILTER_INFO_LINKED filtersHead; // The linked list of filters.
	EX_PUSH_LOCK filtersLock; // The lock protecting the linked list of filters.
	BOOLEAN destroying; // This boolean indicates to functions that a lock should not be held as we are in the process of destruction.

public:
	StringFilters();
	~StringFilters();

	ULONG AddFilter(
		_In_ WCHAR* MatchString,
		_In_ ULONG OperationFlag
		);
	BOOLEAN RemoveFilter(
		_In_ ULONG FilterId
		);
	ULONG GetFilters(
		_In_ ULONG SkipFilters,
		_Inout_ PFILTER_INFO Filters,
		_In_ ULONG FiltersSize
		);

	BOOLEAN MatchesFilter(
		_In_ WCHAR* StrToCmp,
		_In_ ULONG OperationFlag
		);

	ULONG filtersCount;	// Count of filters in the linked-list.
} STRING_FILTERS, *PSTRING_FILTERS;

#define FILTER_INFO_TAG 'iFmP'