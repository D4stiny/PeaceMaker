/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 * 
 * COPYRIGHT Bill Demirkapi 2020
 */
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
	
	VOID WalkAndResolveStack (
		_Inout_ PSTACK_RETURN_INFO* ResolvedStack,
		_Inout_ ULONG* ResolvedStackSize,
		_In_ ULONG ResolvedStackTag
		);

	VOID ResolveAddressModule (
		_In_ PVOID Address,
		_Inout_ PSTACK_RETURN_INFO StackReturnInfo
		);
} STACK_WALKER, *PSTACK_WALKER;

#define STACK_WALK_ARRAY_TAG 'aSmP'
#define STACK_WALK_MAPPED_NAME 'nMmP'