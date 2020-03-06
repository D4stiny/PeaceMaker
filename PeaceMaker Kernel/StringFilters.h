/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 * 
 * COPYRIGHT Bill Demirkapi 2020
 */
#pragma once
#include "common.h"
#include "shared.h"

typedef struct FilterInfoLinked
{
	LIST_ENTRY ListEntry;	// The list entry used to iterate multiple filters.
	FILTER_INFO Filter;		// The filter itself.
} FILTER_INFO_LINKED, *PFILTER_INFO_LINKED;

typedef struct FilterStore
{
	ULONG FilterCount;		// Number of filters in the following array.
	FILTER_INFO Filters[1];	// Dynamically-sized array based on FilterCount member.
} FILTER_STORE, *PFILTER_STORE;

#define FILTER_STORE_SIZE(filterCount) sizeof(FILTER_STORE) + (sizeof(FILTER_INFO) * (filterCount - 1))

typedef class StringFilters
{
	PFILTER_INFO_LINKED filtersHead; // The linked list of filters.
	EX_PUSH_LOCK filtersLock; // The lock protecting the linked list of filters.
	BOOLEAN destroying; // This boolean indicates to functions that a lock should not be held as we are in the process of destruction.
	UNICODE_STRING driverRegistryPath; // Used for filter persistence across reboots.
	UNICODE_STRING filterStoreValueName; // Used for filter persistence across reboots.
	STRING_FILTER_TYPE filterType; // What type of filter this class stores.
public:
	StringFilters(
		_In_ STRING_FILTER_TYPE FilterType,
		_In_ PUNICODE_STRING RegistryPath,
		_In_ CONST WCHAR* FilterStoreName
		);
	~StringFilters();

	ULONG AddFilter(
		_In_ WCHAR* MatchString,
		_In_ ULONG OperationFlag,
		_In_ BOOLEAN SaveFilters = TRUE
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

	BOOLEAN SaveFilters(
		VOID
		);

	BOOLEAN RestoreFilters(
		VOID
		);

	ULONG filtersCount;	// Count of filters in the linked-list.
} STRING_FILTERS, *PSTRING_FILTERS;

#define FILTER_INFO_TAG 'iFmP'