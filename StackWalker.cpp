#include "StackWalker.h"

/**
	Search the current process to see if any modules contain the address.
	@param Address - The address to search for.
	@param StackReturnInfo - The structure to be populated.
*/
VOID
StackWalker::ResolveAddressModule (
	_In_ PVOID Address,
	_Inout_ PSTACK_RETURN_INFO StackReturnInfo
	)
{
	PEPROCESS currentProcess;
	PPEB currentProcessPeb;
	PPEB_LDR_DATA currentProcessLdr;
	PLIST_ENTRY currentEntry;
	PLDR_MODULE currentModuleEntry;
	ULONG64 moduleEnd;

	currentProcess = PsGetCurrentProcess();

	//
	// Not sure how this could happen.
	//
	NT_ASSERT(currentProcess);

	//
	// Always need to have a try/catch when dealing with user-mode pointers.
	//
	__try
	{
		//
		// We can be reckless with memory pointers here cause everything is caught.
		//
		currentProcessPeb = PsGetProcessPeb(currentProcess);
		ProbeForRead(currentProcessPeb, sizeof(*currentProcessPeb), sizeof(ULONG));

		currentProcessLdr = currentProcessPeb->Ldr;
		ProbeForRead(currentProcessLdr, sizeof(*currentProcessLdr), sizeof(ULONG));

		currentEntry = currentProcessLdr->InLoadOrderModuleList.Flink;
		currentModuleEntry = NULL;

		//
		// Iterate every module and see if our address is contained in it.
		//
		while (currentEntry && currentEntry != &currentProcessLdr->InLoadOrderModuleList)
		{
			ProbeForRead(currentEntry, sizeof(*currentEntry), sizeof(ULONG));

			currentModuleEntry = CONTAINING_RECORD(currentEntry, LDR_MODULE, InLoadOrderModuleList);
			moduleEnd = RCAST<ULONG64>(currentModuleEntry->BaseAddress) + currentModuleEntry->SizeOfImage;

			//
			// Check if address in range of module.
			//
			if(currentModuleEntry->BaseAddress <= Address &&
			   moduleEnd >= RCAST<ULONG64>(Address))
			{
				//
				// Fill out the structure.
				//
				StackReturnInfo->MemoryInModule = TRUE;
				StackReturnInfo->BinaryOffset = RCAST<ULONG64>(Address) - RCAST<ULONG64>(currentModuleEntry->BaseAddress);
				RtlStringCbCopyW(StackReturnInfo->BinaryPath, sizeof(StackReturnInfo->BinaryPath), currentModuleEntry->FullDllName.Buffer);
			}

			currentEntry = currentEntry->Flink;
		}
	}
	__except (1)
	{}
}

/**
	Check if the memory pointed by address is executable.
	@param Address - The address to check.
	@return Whether or not the memory is executable.
*/
BOOLEAN
StackWalker::IsAddressExecutable (
	_In_ PVOID Address
	)
{
	NTSTATUS status;
	HANDLE currentProcessHandle;
	MEMORY_BASIC_INFORMATION memoryBasicInformation;
	BOOLEAN executable;

	executable = FALSE;
	memset(&memoryBasicInformation, 0, sizeof(memoryBasicInformation));

	status = ObOpenObjectByPointer(PsGetCurrentProcess(), OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, GENERIC_ALL, *PsProcessType, KernelMode, &currentProcessHandle);
	if (NT_SUCCESS(status) == FALSE)
	{
		DBGPRINT("StackWalker!IsAddressExecutable: Failed to open handle to process with status 0x%X.", status);
		goto Exit;
	}

	//
	// Query the basic information about the memory.
	//
	status = ZwQueryVirtualMemory(currentProcessHandle, Address, MemoryBasicInformation, &memoryBasicInformation, sizeof(memoryBasicInformation), NULL);
	if (NT_SUCCESS(status) == FALSE)
	{
		DBGPRINT("StackWalker!IsAddressExecutable: Failed to query virtual memory for address 0x%llx with status 0x%X.", RCAST<ULONG64>(Address), status);
		goto Exit;
	}

	//
	// Check if the protection flags specifies executable.
	//
	executable = FlagOn(memoryBasicInformation.AllocationProtect, PAGE_EXECUTE) ||
				 FlagOn(memoryBasicInformation.AllocationProtect, PAGE_EXECUTE_READ) ||
				 FlagOn(memoryBasicInformation.AllocationProtect, PAGE_EXECUTE_READWRITE) ||
				 FlagOn(memoryBasicInformation.AllocationProtect, PAGE_EXECUTE_WRITECOPY);
Exit:
	if (currentProcessHandle)
	{
		ZwClose(currentProcessHandle);
	}
	return NT_SUCCESS(status) && executable;
}

/**
	Walk the stack of the current thread and resolve the module associated with the return addresses.
	@param SkipFrames - The number of stack frames to skip.
	@param ResolvedStack - Caller-supplied array of return address information that this function populates.
	@param ResolvedStackSize - The maximum number of return addresses to resolve.
	@return The number of return addresses resolved.
*/
ULONG
StackWalker::WalkAndResolveStack (
	_Inout_ STACK_RETURN_INFO ResolvedStack[],
	_In_ ULONG ResolvedStackSize
	)
{
	PVOID* stackReturnPtrs;
	ULONG capturedReturnPtrs;
	ULONG i;
	
	capturedReturnPtrs = 0;
	memset(ResolvedStack, 0, sizeof(STACK_RETURN_INFO) * ResolvedStackSize);

	//
	// Allocate space for the return addresses.
	//
	stackReturnPtrs = RCAST<PVOID*>(ExAllocatePoolWithTag(PagedPool, sizeof(PVOID) * ResolvedStackSize, STACK_WALK_ARRAY_TAG));
	if (stackReturnPtrs == NULL)
	{
		DBGPRINT("StackWalker!WalkAndResolveStack: Failed to allocate space for temporary stack array.");
		goto Exit;
	}

	//
	// Get the return addresses leading up to this call.
	//
	capturedReturnPtrs = RtlWalkFrameChain(stackReturnPtrs, ResolvedStackSize, 1);
	if (capturedReturnPtrs == 0)
	{
		DBGPRINT("StackWalker!WalkAndResolveStack: Failed to walk the stack.");
		goto Exit;
	}

	NT_ASSERT(capturedReturnPtrs < ResolvedStackSize);
	//
	// Iterate each return address and fill out the struct.
	//
	for (i = 0; i < capturedReturnPtrs; i++)
	{
		ResolvedStack[i].RawAddress = stackReturnPtrs[i];

		//
		// If the memory isn't executable or is in kernel, it's not worth our time.
		//
		if (RCAST<ULONG64>(stackReturnPtrs[i]) < MmUserProbeAddress && this->IsAddressExecutable(stackReturnPtrs[i]))
		{
			ResolvedStack[i].ExecutableMemory = TRUE;
			this->ResolveAddressModule(stackReturnPtrs[i], &ResolvedStack[i]);
		}
	}
Exit:
	if (stackReturnPtrs)
	{
		ExFreePoolWithTag(stackReturnPtrs, STACK_WALK_ARRAY_TAG);
	}
	return capturedReturnPtrs;
}