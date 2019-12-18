#include "Filters.h"

/**
	Initialize the CUSTOM_FILTERS class by initializing the linked list's lock.
*/
CUSTOM_FILTERS::CUSTOM_FILTERS(
	VOID
	)
{
	//
	// Initialize the lock for the filters.
	//
	FltInitializePushLock(&this->filtersLock);

	this->filtersHead = NULL;
	this->destroying = FALSE;
}

/**
	Destroy the CustomFilters class by clearing the filters linked list and deleting the associated lock.
*/
VOID
CUSTOM_FILTERS::DestructCustomFilters (
	VOID
	)
{
	PFILTERINFO currentFilter;
	PFILTERINFO nextFilter;

	PAGED_CODE();

	//
	// Set destroying to TRUE so that no other threads can get a lock.
	//
	this->destroying = TRUE;

	//
	// Acquire an exclusive lock to push out other threads.
	//
	FltAcquirePushLockExclusive(&this->filtersLock);

	//
	// Release the lock.
	//
	FltReleasePushLock(&this->filtersLock);

	//
	// Delete the lock for the filters.
	//
	FltDeletePushLock(&this->filtersLock);

	//
	// Go through each filter and free it.
	//
	currentFilter = this->filtersHead;
	while (currentFilter)
	{
		nextFilter = RCAST<PFILTERINFO>(currentFilter->SingleListEntry.Next);

		//
		// Free the filter.
		//
		ExFreePoolWithTag(SCAST<PVOID>(currentFilter), FILTER_INFO_TAG);

		currentFilter = nextFilter;
	}
}

/**
	Add a filter to the linked list of filters.
	@param MatchString - The string to filter with.
	@return A random identifier required for future operations with the new filter.
*/
ULONG
CUSTOM_FILTERS::AddFilter (
	WCHAR* MatchString
	)
{
	PFILTERINFO currentFilter;
	PFILTERINFO newFilter;
	LARGE_INTEGER currentTime;
	ULONG epochSeconds;

	PAGED_CODE();

	if (this->destroying)
	{
		return NULL;
	}

	//
	// Get an exclusive lock because we're modifying the filters linked list.
	//
	FltAcquirePushLockExclusive(&this->filtersLock);

	//
	// Allocate space for the new filter.
	//
	newFilter = RCAST<PFILTERINFO>(ExAllocatePoolWithTag(PagedPool, sizeof(FILTERINFO), FILTER_INFO_TAG));
	if (newFilter == NULL)
	{
		DBGPRINT("Failed to allocate space for filter info.");
		goto Exit;
	}

	memset(RCAST<PVOID>(newFilter), 0, sizeof(FILTERINFO));

	currentFilter = this->filtersHead;
	while (currentFilter)
	{
		//
		// Break if we're at the last element.
		//
		if (currentFilter->SingleListEntry.Next == NULL)
		{
			break;
		}
		currentFilter = RCAST<PFILTERINFO>(currentFilter->SingleListEntry.Next);
	}

	//
	// Set the next entry of the filter list to the new filter.
	//
	if (currentFilter)
	{
		currentFilter->SingleListEntry.Next = RCAST<PSINGLE_LIST_ENTRY>(newFilter);
	}
	else
	{
		//
		// Otherwise, we need to initialize the head.
		//
		this->filtersHead = newFilter;
	}

	//
	// Generate a pseudo-random ID for the filter using the system time.
	//
	KeQuerySystemTime(&currentTime);
	RtlTimeToSecondsSince1970(&currentTime, &epochSeconds);
	newFilter->Id = (RtlRandomEx(&epochSeconds) + 1) % MAXDWORD32;
	
	//
	// Copy the filter string to the new filter.
	//
	wcsncpy_s(newFilter->MatchString, MatchString, MAX_PATH);

Exit:
	//
	// New filter has been initialized, release the lock.
	//
	FltReleasePushLock(&this->filtersLock);

	if (newFilter)
	{
		return newFilter->Id;
	}

	//
	// The ID cannot be 0 because we add 1.
	//
	return NULL;
}

/**
	Remove a filter from the linked list of filters.
	@param FilterId - The unique filter ID of the filter to delete.
	@return Whether or not deletion was successful.
*/
BOOLEAN
CUSTOM_FILTERS::RemoveFilter(
	ULONG FilterId
	)
{
	BOOLEAN filterDeleted;
	PFILTERINFO currentFilter;
	PFILTERINFO previousFilter;

	PAGED_CODE();

	currentFilter = NULL;
	filterDeleted = FALSE;

	if (this->destroying)
	{
		return FALSE;
	}

	//
	// Get an exclusive lock because we're modifying the filters linked list.
	//
	FltAcquirePushLockExclusive(&this->filtersLock);

	//
	// Check if we have a single filter already.
	//
	if (this->filtersHead)
	{
		currentFilter = this->filtersHead;
		previousFilter = currentFilter;

		//
		// Check if the filter to remove is the head.
		//
		if (currentFilter->Id == FilterId)
		{
			this->filtersHead = NULL;
			goto Exit;
		}

		while (currentFilter)
		{
			if (currentFilter->Id == FilterId)
			{
				break;
			}
			previousFilter = currentFilter;
			currentFilter = RCAST<PFILTERINFO>(currentFilter->SingleListEntry.Next);
		}

		//
		// Set the previous element to point at the element after the one we're deleting.
		//
		previousFilter->SingleListEntry.Next = currentFilter->SingleListEntry.Next;
	}
Exit:
	//
	// Release the lock.
	//
	FltReleasePushLock(&this->filtersLock);

	if (currentFilter)
	{
		//
		// Free the filter.
		//
		ExFreePoolWithTag(SCAST<PVOID>(currentFilter), FILTER_INFO_TAG);
	}

	return filterDeleted;
}

/**
	Check if a string contains any filtered phrases.
	@param StrToCmp - The string to search.
	@return Whether or not there was a filter that matched.
*/
BOOLEAN
CUSTOM_FILTERS::MatchesFilter (
	WCHAR* StrToCmp
	)
{
	BOOLEAN filterMatched;
	PFILTERINFO currentFilter;
	WCHAR tempStrToCmp[MAX_PATH];
	INT i;

	PAGED_CODE();

	filterMatched = FALSE;

	//
	// Copy the string to compare so we don't modify the original string.
	//
	wcsncpy_s(tempStrToCmp, StrToCmp, MAX_PATH);

	//
	// Make the input string lowercase.
	//
	i = 0;
	while (tempStrToCmp[i])
	{
		tempStrToCmp[i] = towlower(tempStrToCmp[i]);
		i++;
	}

	//
	// Acquire a shared lock to iterate filters.
	//
	FltAcquirePushLockShared(&this->filtersLock);

	//
	// Iterate filters for a match.
	//
	currentFilter = this->filtersHead;
	while (currentFilter)
	{
		//
		// Check if the string to compare contains the filter.
		//
		//DBGPRINT("Filter 0x%llx, MatchString 0x%llx", currentFilter, currentFilter->MatchString);
		if (wcsstr(RCAST<CONST WCHAR*>(&tempStrToCmp), RCAST<CONST WCHAR*>(&currentFilter->MatchString)) != NULL)
		{
			filterMatched = TRUE;
			goto Exit;
		}
		currentFilter = RCAST<PFILTERINFO>(currentFilter->SingleListEntry.Next);
	}
Exit:
	FltReleasePushLock(&this->filtersLock);
	return filterMatched;
}