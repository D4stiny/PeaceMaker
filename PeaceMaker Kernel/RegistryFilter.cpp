/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 * 
 * COPYRIGHT Bill Demirkapi 2020
 */
#include "RegistryFilter.h"

LARGE_INTEGER RegistryBlockingFilter::RegistryFilterCookie;
PDETECTION_LOGIC RegistryBlockingFilter::detector;
STACK_WALKER RegistryBlockingFilter::walker;
PSTRING_FILTERS RegistryBlockingFilter::RegistryStringFilters;

/**
	Initializes the necessary components of the registry filter.
	@param DriverObject - The object of the driver necessary for mini-filter initialization.
	@param RegistryPath - The registry path of the driver.
	@param Detector - Detection instance used to analyze untrusted operations.
	@param InitializeStatus - Status of initialization.
*/
RegistryBlockingFilter::RegistryBlockingFilter(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath,
	_In_ PDETECTION_LOGIC Detector,
	_Out_ NTSTATUS* InitializeStatus
	)
{
	UNICODE_STRING filterAltitude;

	RegistryBlockingFilter::RegistryStringFilters = new (NonPagedPool, STRING_REGISTRY_FILTERS_TAG) StringFilters(RegistryFilter, RegistryPath, L"RegistryFilterStore");
	if (RegistryBlockingFilter::RegistryStringFilters == NULL)
	{
		DBGPRINT("RegistryBlockingFilter!RegistryBlockingFilter: Failed to allocate memory for string filters.");
		*InitializeStatus = STATUS_NO_MEMORY;
		return;
	}
	//
	// Restore existing filters.
	//
	RegistryBlockingFilter::RegistryStringFilters->RestoreFilters();

	//
	// Put our altitude into a UNICODE_STRING.
	//
	RtlInitUnicodeString(&filterAltitude, FILTER_ALTITUDE);

	//
	// Register our registry callback.
	//
	*InitializeStatus = CmRegisterCallbackEx(RCAST<PEX_CALLBACK_FUNCTION>(RegistryBlockingFilter::RegistryCallback), &filterAltitude, DriverObject, NULL, &RegistryFilterCookie, NULL);

	//
	// Set the detector.
	//
	RegistryBlockingFilter::detector = Detector;
}

/**
	Free data members that were dynamically allocated.
*/
RegistryBlockingFilter::~RegistryBlockingFilter()
{
	//
	// Remove the registry callback.
	//
	CmUnRegisterCallback(this->RegistryFilterCookie);

	//
	// Make sure to deconstruct the class.
	//
	RegistryBlockingFilter::RegistryStringFilters->~StringFilters();
	ExFreePoolWithTag(RegistryBlockingFilter::RegistryStringFilters, STRING_REGISTRY_FILTERS_TAG);
}

/**
	Return the string filters used in the registry filter.
	@return String filters for registry operations.
*/
PSTRING_FILTERS RegistryBlockingFilter::GetStringFilters()
{
	return RegistryBlockingFilter::RegistryStringFilters;
}

/**
	Function that decides whether or not to block a registry operation.
	@param KeyObject - The registry key of the operation.
	@param ValueName - The name of the registry value specified by the operation.
	@param OperationFlag - The flags of the operation (i.e WRITE/DELETE).
	@return Whether or not to block the operation.
*/
BOOLEAN
RegistryBlockingFilter::BlockRegistryOperation (
	_In_ PVOID KeyObject,
	_In_ PUNICODE_STRING ValueName,
	_In_ ULONG OperationFlag
	)
{
	BOOLEAN blockOperation;
	NTSTATUS internalStatus;
	HANDLE keyHandle;
	PKEY_NAME_INFORMATION pKeyNameInformation;
	ULONG returnLength;
	ULONG fullKeyValueLength;
	PWCHAR tempValueName;
	PWCHAR fullKeyValueName;

	UNICODE_STRING registryOperationPath;
	PUNICODE_STRING callerProcessPath;
	PSTACK_RETURN_INFO registryOperationStack;
	ULONG registryOperationStackSize;

	blockOperation = FALSE;
	registryOperationStackSize = MAX_STACK_RETURN_HISTORY;
	keyHandle = NULL;
	returnLength = NULL;
	pKeyNameInformation = NULL;
	tempValueName = NULL;
	fullKeyValueName = NULL;

	if (ValueName == NULL || ValueName->Length == 0 || ValueName->Buffer == NULL || ValueName->Length > (NTSTRSAFE_UNICODE_STRING_MAX_CCH * sizeof(WCHAR)))
	{
		DBGPRINT("RegistryBlockingFilter!BlockRegistryOperation: ValueName is NULL.");
		goto Exit;
	}

	tempValueName = RCAST<PWCHAR>(ExAllocatePoolWithTag(NonPagedPoolNx, ValueName->Length, REGISTRY_KEY_NAME_TAG));
	if (tempValueName == NULL)
	{
		DBGPRINT("RegistryBlockingFilter!BlockRegistryOperation: Failed to allocate memory for value name with size 0x%X.", ValueName->Length);
		goto Exit;
	}

	//
	// There can be some wonky exceptions with weird input,
	// just in case we don't handle something is a simple
	// catch all.
	//
	__try {
		//
		// Open the registry key.
		//
		internalStatus = ObOpenObjectByPointer(KeyObject, OBJ_KERNEL_HANDLE, NULL, GENERIC_ALL, *CmKeyObjectType, KernelMode, &keyHandle);
		if (NT_SUCCESS(internalStatus) == FALSE)
		{
			DBGPRINT("RegistryBlockingFilter!BlockRegistryOperation: Failed to open a handle to a key object with status 0x%X.", internalStatus);
			goto Exit;
		}

		ZwQueryKey(keyHandle, KeyNameInformation, NULL, 0, &returnLength);
		if (returnLength == 0)
		{
			DBGPRINT("RegistryBlockingFilter!BlockRegistryOperation: Failed to determine size of key name.");
			goto Exit;
		}

		returnLength += 1; // For null terminator.
		pKeyNameInformation = RCAST<PKEY_NAME_INFORMATION>(ExAllocatePoolWithTag(PagedPool, returnLength, REGISTRY_KEY_NAME_TAG));
		if (pKeyNameInformation == NULL)
		{
			DBGPRINT("RegistryBlockingFilter!BlockRegistryOperation: Failed to allocate memory for key name with size 0x%X.", returnLength);
			goto Exit;
		}

		//
		// Query the name information of the key to retrieve its name.
		//
		internalStatus = ZwQueryKey(keyHandle, KeyNameInformation, pKeyNameInformation, returnLength, &returnLength);
		if (NT_SUCCESS(internalStatus) == FALSE)
		{
			DBGPRINT("RegistryBlockingFilter!BlockRegistryOperation: Failed to query name of key object with status 0x%X.", internalStatus);
			goto Exit;
		}

		//
		// Allocate space for key name, a backslash, the value name, and the null-terminator.
		//
		fullKeyValueLength = pKeyNameInformation->NameLength + 2 + ValueName->Length + 1000;
		fullKeyValueName = RCAST<PWCHAR>(ExAllocatePoolWithTag(NonPagedPoolNx, fullKeyValueLength, REGISTRY_KEY_NAME_TAG));
		if (fullKeyValueName == NULL)
		{
			DBGPRINT("RegistryBlockingFilter!BlockRegistryOperation: Failed to allocate memory for full key/value name with size 0x%X.", fullKeyValueLength);
			goto Exit;
		}

		//
		// Copy the key name.
		//
		internalStatus = RtlStringCbCopyNW(fullKeyValueName, fullKeyValueLength, RCAST<PCWSTR>(&pKeyNameInformation->Name), pKeyNameInformation->NameLength);
		if (NT_SUCCESS(internalStatus) == FALSE)
		{
			DBGPRINT("RegistryBlockingFilter!BlockRegistryOperation: Failed to copy key name with status 0x%X.", internalStatus);
			goto Exit;
		}

		//
		// Concatenate the backslash.
		//
		internalStatus = RtlStringCbCatW(fullKeyValueName, fullKeyValueLength, L"\\");
		if (NT_SUCCESS(internalStatus) == FALSE)
		{
			DBGPRINT("RegistryBlockingFilter!BlockRegistryOperation: Failed to concatenate backslash with status 0x%X.", internalStatus);
			goto Exit;
		}

		//
		// Concatenate the value name.
		//
		internalStatus = RtlStringCbCatNW(fullKeyValueName, fullKeyValueLength, ValueName->Buffer, ValueName->Length);
		if (NT_SUCCESS(internalStatus) == FALSE)
		{
			DBGPRINT("RegistryBlockingFilter!BlockRegistryOperation: Failed to concatenate value name with status 0x%X.", internalStatus);
			goto Exit;
		}

		blockOperation = RegistryBlockingFilter::RegistryStringFilters->MatchesFilter(fullKeyValueName, OperationFlag);

		//DBGPRINT("RegistryBlockingFilter!BlockRegistryOperation: Full name: %S.", fullKeyValueName);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{}

	if (blockOperation)
	{
		//
		// Grab the caller's path.
		//
		ImageHistoryFilter::GetProcessImageFileName(PsGetCurrentProcessId(), &callerProcessPath);

		//
		// Walk the stack.
		//
		RegistryBlockingFilter::walker.WalkAndResolveStack(&registryOperationStack, &registryOperationStackSize, STACK_HISTORY_TAG);

		NT_ASSERT(registryOperationStack);

		//
		// Only if we successfully walked the stack, report the violation.
		//
		if (registryOperationStack != NULL && registryOperationStackSize != 0)
		{
			//
			// Convert the registry path to a unicode string.
			//
			RtlInitUnicodeString(&registryOperationPath, fullKeyValueName);

			//
			// Report the violation.
			//
			RegistryBlockingFilter::detector->ReportFilterViolation(RegistryFilterMatch, PsGetCurrentProcessId(), callerProcessPath, &registryOperationPath, registryOperationStack, registryOperationStackSize);

			//
			// Clean up.
			//
			ExFreePoolWithTag(registryOperationStack, STACK_HISTORY_TAG);
		}

		ExFreePoolWithTag(callerProcessPath, IMAGE_NAME_TAG);
	}
Exit:
	if (tempValueName)
	{
		ExFreePoolWithTag(tempValueName, REGISTRY_KEY_NAME_TAG);
	}
	if (fullKeyValueName)
	{
		ExFreePoolWithTag(fullKeyValueName, REGISTRY_KEY_NAME_TAG);
	}
	if (pKeyNameInformation)
	{
		ExFreePoolWithTag(pKeyNameInformation, REGISTRY_KEY_NAME_TAG);
	}
	if (keyHandle)
	{
		ZwClose(keyHandle);
	}
	return blockOperation;
}

/**
	The callback for registry operations. If necessary, blocks certain operations on protected keys/values.
	@param CallbackContext - Unreferenced parameter.
	@param OperationClass - The type of registry operation.
	@param Argument2 - A pointer to the structure associated with the operation.
	@return The status of the registry operation.
*/
NTSTATUS RegistryBlockingFilter::RegistryCallback (
	_In_ PVOID CallbackContext,
	_In_ REG_NOTIFY_CLASS OperationClass, 
	_In_ PVOID Argument2
	)
{
	UNREFERENCED_PARAMETER(CallbackContext);
	NTSTATUS returnStatus;
	PREG_SET_VALUE_KEY_INFORMATION setValueInformation;
	PREG_DELETE_VALUE_KEY_INFORMATION deleteValueInformation;

	returnStatus = STATUS_SUCCESS;

	//
	// PeaceMaker is not designed to block kernel operations.
	//
	if (ExGetPreviousMode() != KernelMode)
	{
		switch (OperationClass)
		{
		case RegNtPreSetValueKey:
			setValueInformation = RCAST<PREG_SET_VALUE_KEY_INFORMATION>(Argument2);
			if (BlockRegistryOperation(setValueInformation->Object, setValueInformation->ValueName, FILTER_FLAG_WRITE))
			{
				DBGPRINT("RegistryBlockingFilter!RegistryCallback: Detected RegNtPreSetValueKey of %wZ. Prevented set!", setValueInformation->ValueName);
				returnStatus = STATUS_ACCESS_DENIED;
			}
			break;
		case RegNtPreDeleteValueKey:
			deleteValueInformation = RCAST<PREG_DELETE_VALUE_KEY_INFORMATION>(Argument2);
			if (BlockRegistryOperation(deleteValueInformation->Object, deleteValueInformation->ValueName, FILTER_FLAG_DELETE))
			{
				DBGPRINT("RegistryBlockingFilter!RegistryCallback: Detected RegNtPreDeleteValueKey of %wZ. Prevented rewrite!", deleteValueInformation->ValueName);
				returnStatus = STATUS_ACCESS_DENIED;
			}
			break;
		}
	}

	return returnStatus;
}