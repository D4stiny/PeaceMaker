#pragma once
#include "common.h"
#include "shared.h"

typedef class StackWalker
{
	BOOLEAN IsAddressExecutable (
		_In_ PVOID Address
		);

public:
	StackWalker() {};
	
	ULONG WalkAndResolveStack (
		_Inout_ STACK_RETURN_INFO ResolvedStack[],
		 _In_ ULONG ResolvedStackSize
		);

	VOID ResolveAddressModule (
		_In_ PVOID Address,
		_Inout_ PSTACK_RETURN_INFO StackReturnInfo
		);
} STACK_WALKER, *PSTACK_WALKER;

#define STACK_WALK_ARRAY_TAG 'aSmP'
#define STACK_WALK_MAPPED_NAME 'nMmP'