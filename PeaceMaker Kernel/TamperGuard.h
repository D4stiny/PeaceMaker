/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 * 
 * COPYRIGHT Bill Demirkapi 2020
 */
#pragma once
#include "common.h"
#include "shared.h"

typedef class TamperGuard
{
	static OB_PREOP_CALLBACK_STATUS PreOperationCallback(_In_ PVOID RegistrationContext,
														 _In_ POB_PRE_OPERATION_INFORMATION OperationInformation
														 );
	OB_CALLBACK_REGISTRATION ObRegistrationInformation;
	OB_OPERATION_REGISTRATION ObOperationRegistration[2];
	PVOID RegistrationHandle;

	static HANDLE ProtectedProcessId;
public:
	TamperGuard(_Out_ NTSTATUS* InitializeStatus
				);
	~TamperGuard(VOID);

	VOID UpdateProtectedProcess(_In_ HANDLE NewProcessId
								);
} TAMPER_GUARD, *PTAMPER_GUARD;

#define PROCESS_TERMINATE                  (0x0001)  
#define THREAD_TERMINATE                 (0x0001)  