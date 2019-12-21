#include "StringFilters.h"

/**
	Initialize the CUSTOM_FILTERS class by initializing the linked list's lock.
*/
StringFilters::StringFilters()
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
StringFilters::~StringFilters()
{
	PLIST_ENTRY currentFilter;

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
	if (this->filtersHead)
	{
		while (IsListEmpty(RCAST<PLIST_ENTRY>(this->filtersHead)) == FALSE)
		{
			currentFilter = RemoveHeadList(RCAST<PLIST_ENTRY>(this->filtersHead));
			//
			// Free the filter.
			//
			ExFreePoolWithTag(SCAST<PVOID>(currentFilter), FILTER_INFO_TAG);
		}
	}
}

/**
	Add a filter to the linked list of filters.
	@param MatchString - The string to filter with.
	@param OperationFlag - Specifies what operations this filter should be used for.
	@return A random identifier required for future operations with the new filter.
*/
ULONG
StringFilters::AddFilter (
	WCHAR* MatchString,
	ULONG OperationFlag
	)
{
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
	newFilter = RCAST<PFILTERINFO>(ExAllocatePoolWithTag(NonPagedPool, sizeof(FILTERINFO), FILTER_INFO_TAG));
	if (newFilter == NULL)
	{
		DBGPRINT("Failed to allocate space for filter info.");
		goto Exit;
	}

	memset(RCAST<PVOID>(newFilter), 0, sizeof(FILTERINFO));

	//
	// Check if the list has been initialized.
	//
	if (this->filtersHead == NULL)
	{
		this->filtersHead = newFilter;
		InitializeListHead(RCAST<PLIST_ENTRY>(this->filtersHead));
	}
	//
	// Otherwise, just append the element to the end of the list.
	//
	else
	{
		InsertTailList(RCAST<PLIST_ENTRY>(this->filtersHead), RCAST<PLIST_ENTRY>(newFilter));
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

	//
	// Set the operation flags for this filter.
	//
	newFilter->Flags = OperationFlag;

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
StringFilters::RemoveFilter(
	ULONG FilterId
	)
{
	BOOLEAN filterDeleted;
	PFILTERINFO currentFilter;

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

		//
		// Check if the filter to remove is the head.
		//
		if (currentFilter->Id == FilterId)
		{
			this->filtersHead = NULL;
			goto Exit;
		}

		do
		{
			if (currentFilter->Id == FilterId)
			{
				break;
			}
			currentFilter = RCAST<PFILTERINFO>(currentFilter->ListEntry.Flink);
		} while (currentFilter && currentFilter != this->filtersHead);
		
		//
		// Remove the entry from the list.
		//
		if (currentFilter)
		{
			RemoveEntryList(RCAST<PLIST_ENTRY>(currentFilter));
		}
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
	@param OperationFlag - Specify FILTER_FLAG_X's to match certain filters for a variety of operations.
	@return Whether or not there was a filter that matched.
*/
BOOLEAN
StringFilters::MatchesFilter (
	WCHAR* StrToCmp,
	ULONG OperationFlag
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
	if (this->filtersHead)
	{
		currentFilter = this->filtersHead;
		do
		{
			//
			// Check if the string to compare contains the filter.
			//
			if ((currentFilter->Flags & OperationFlag) &&
				(wcsstr(RCAST<CONST WCHAR*>(&tempStrToCmp), RCAST<CONST WCHAR*>(&currentFilter->MatchString)) != NULL))
			{
				filterMatched = TRUE;
				goto Exit;
			}
			currentFilter = RCAST<PFILTERINFO>(currentFilter->ListEntry.Flink);
		} while (currentFilter && currentFilter != this->filtersHead);
	}
Exit:
	FltReleasePushLock(&this->filtersLock);
	return filterMatched;
}