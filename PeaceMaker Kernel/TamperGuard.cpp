/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 * 
 * COPYRIGHT Bill Demirkapi 2020
 */
#include "TamperGuard.h"

HANDLE TamperGuard::ProtectedProcessId;

/**
	Initialize tamper protection. This constructor will create appropriate callbacks.
	@param InitializeStatus - Status of initialization.
*/
TamperGuard::TamperGuard (
	_Out_ NTSTATUS* InitializeStatus
	)
{
	UNICODE_STRING CallbackAltitude;

	RtlInitUnicodeString(&CallbackAltitude, FILTER_ALTITUDE);

	ObRegistrationInformation.Version = OB_FLT_REGISTRATION_VERSION;
	ObRegistrationInformation.OperationRegistrationCount = 2;
	ObRegistrationInformation.Altitude = CallbackAltitude;
	ObRegistrationInformation.RegistrationContext = NULL;
	ObRegistrationInformation.OperationRegistration = ObOperationRegistration;

	//
	// We want to protect both the process and the threads of the protected process.
	//
	ObOperationRegistration[0].ObjectType = PsProcessType;
	ObOperationRegistration[0].Operations |= OB_OPERATION_HANDLE_CREATE;
	ObOperationRegistration[0].Operations |= OB_OPERATION_HANDLE_DUPLICATE;
	ObOperationRegistration[0].PreOperation = PreOperationCallback;

	ObOperationRegistration[1].ObjectType = PsThreadType;
	ObOperationRegistration[1].Operations |= OB_OPERATION_HANDLE_CREATE;
	ObOperationRegistration[1].Operations |= OB_OPERATION_HANDLE_DUPLICATE;
	ObOperationRegistration[1].PreOperation = PreOperationCallback;

	//
	// Actually register the callbacks.
	//
	*InitializeStatus = ObRegisterCallbacks(&ObRegistrationInformation, &RegistrationHandle);
	if (NT_SUCCESS(*InitializeStatus) == FALSE)
	{
		DBGPRINT("TamperGuard!TamperGuard: Failed to register object callbacks with status 0x%X.", *InitializeStatus);
	}
}

/**
	Destruct tamper guard members.
*/
TamperGuard::~TamperGuard (
	VOID
	)
{
	ObUnRegisterCallbacks(RegistrationHandle);
}

/**
	Update the process to protect.
	@param NewProcessId - The new process to protect from tampering.
*/
VOID
TamperGuard::UpdateProtectedProcess (
	_In_ HANDLE NewProcessId
	)
{
	TamperGuard::ProtectedProcessId = NewProcessId;
}

/**
	Filter for certain operations on a protected process.
	@param RegistrationContext - Always NULL.
	@param OperationInformation - Information about the current operation.
*/
OB_PREOP_CALLBACK_STATUS
TamperGuard::PreOperationCallback (
	_In_ PVOID RegistrationContext,
	_In_ POB_PRE_OPERATION_INFORMATION OperationInformation
	)
{
	HANDLE callerProcessId;
	HANDLE targetProcessId;
	ACCESS_MASK targetAccessMask;

	UNREFERENCED_PARAMETER(RegistrationContext);

	callerProcessId = NULL;
	targetProcessId = NULL;
	targetAccessMask = NULL;
	callerProcessId = PsGetCurrentProcessId();

	//
	// Grab the appropriate process IDs based on the operation object type.
	//
	if (OperationInformation->ObjectType == *PsProcessType)
	{
		targetProcessId = PsGetProcessId(RCAST<PEPROCESS>(OperationInformation->Object));
		targetAccessMask = PROCESS_TERMINATE;
	}
	else if (OperationInformation->ObjectType == *PsThreadType)
	{
		targetProcessId = PsGetThreadProcessId(RCAST<PETHREAD>(OperationInformation->Object));
		targetAccessMask = THREAD_TERMINATE;
	}

	//
	// If this is an operation on your own process, ignore it.
	//
	if (callerProcessId == targetProcessId)
	{
		return OB_PREOP_SUCCESS;
	}

	//
	// If the target process isn't the process we're protecting, no issue.
	//
	if (targetProcessId != TamperGuard::ProtectedProcessId)
	{
		return OB_PREOP_SUCCESS;
	}

	//
	// Strip the proper desired access ACCESS_MASK.
	//
	switch (OperationInformation->Operation)
	{
	case OB_OPERATION_HANDLE_CREATE:
		OperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~targetAccessMask;
		break;
	case OB_OPERATION_HANDLE_DUPLICATE:
		OperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess &= ~targetAccessMask;
		break;
	}

	DBGPRINT("TamperGuard!PreOperationCallback: Stripped process 0x%X terminate handle on protected process.", callerProcessId);

	return OB_PREOP_SUCCESS;
}