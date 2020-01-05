#pragma once
#include "common.h"

typedef struct StackReturnInfo
{
	PVOID RawAddress;			// The raw return address.
	BOOLEAN MemoryInModule;		// Whether or not the address is in a loaded module.
	BOOLEAN ExecutableMemory;	// Whether or not the address is in executable memory.
	WCHAR BinaryPath[MAX_PATH];	// The path of the binary this return address specifies.
	ULONG64 BinaryOffset;		// The offset in the binary that the return address refers to.
} STACK_RETURN_INFO, *PSTACK_RETURN_INFO;

typedef class StackWalker
{
	BOOLEAN IsAddressExecutable(_In_ PVOID Address);
	VOID ResolveAddressModule(_In_ PVOID Address,
							  _Inout_ PSTACK_RETURN_INFO StackReturnInfo
								 );

public:
	StackWalker() {};
	
	ULONG WalkAndResolveStack(_Inout_ STACK_RETURN_INFO ResolvedStack[],
							  _In_ ULONG ResolvedStackSize
							  );
} STACK_WALKER, *PSTACK_WALKER;

#define STACK_WALK_ARRAY_TAG 'aSmP'