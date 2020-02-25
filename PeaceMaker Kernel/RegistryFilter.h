/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 * 
 * COPYRIGHT Bill Demirkapi 2020
 */
#pragma once
#include "common.h"
#include "StringFilters.h"
#include "StackWalker.h"
#include "DetectionLogic.h"
#include "ImageHistoryFilter.h"

typedef class RegistryBlockingFilter
{
	static BOOLEAN BlockRegistryOperation(_In_ PVOID KeyObject,
										  _In_ PUNICODE_STRING ValueName,
										  _In_ ULONG OperationFlag
										  );

	static NTSTATUS RegistryCallback(_In_ PVOID CallbackContext,
									 _In_ REG_NOTIFY_CLASS OperationClass,
									 _In_ PVOID Argument2
									);

	//
	// Contains strings to block various registry operations.
	//
	static PSTRING_FILTERS RegistryStringFilters;

	//
	// Cookie used to remove registry callback.
	//
	static LARGE_INTEGER RegistryFilterCookie;

	static STACK_WALKER walker;
	static PDETECTION_LOGIC detector;
public:
	RegistryBlockingFilter (
		_In_ PDRIVER_OBJECT DriverObject,
		_In_ PUNICODE_STRING RegistryPath,
		_In_ PDETECTION_LOGIC Detector,
		_Out_ NTSTATUS* Initialized
		);
	~RegistryBlockingFilter();

	static PSTRING_FILTERS GetStringFilters();
} REGISTRY_BLOCKING_FILTER, *PREGISTRY_BLOCKING_FILTER;

#define STRING_REGISTRY_FILTERS_TAG 'rFmP'
#define REGISTRY_KEY_NAME_TAG 'nKmP'