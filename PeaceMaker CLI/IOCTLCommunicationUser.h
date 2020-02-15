/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 * 
 * COPYRIGHT Bill Demirkapi 2020
 */
#pragma once
#include <Windows.h>
#include <cstdio>
#include "shared.h"

class IOCTLCommunication
{
	HANDLE device;

	BOOLEAN GenericQueryDriver(
		_In_ DWORD IOCTLCode,
		_In_ PVOID Input,
		_In_ DWORD InputLength,
		_Out_ PVOID Output,
		_In_ DWORD OutputLength
		);

public:
	IOCTLCommunication(VOID);
	~IOCTLCommunication(VOID);

	BOOLEAN ConnectDevice(
		VOID
		);
	BOOLEAN QueuedAlerts(
		VOID
		);
	PBASE_ALERT_INFO PopAlert(
		VOID
		);
	PPROCESS_SUMMARY_REQUEST RequestProcessSummary(
		_In_ ULONG SkipCount,
		_In_ ULONG RequestCount
		);
	PPROCESS_DETAILED_REQUEST RequestDetailedProcess(
		_In_ HANDLE ProcessId,
		_In_ ULONG EpochExecutionTime,
		_In_ ULONG MaxImageSize,
		_In_ ULONG MaxStackSize
		);
	ULONG AddFilter(
		_In_ STRING_FILTER_TYPE Type,
		_In_ ULONG Flags,
		_In_ PWCHAR Content,
		_In_ ULONG ContentLength
		);
	LIST_FILTERS_REQUEST RequestFilters(
		_In_ STRING_FILTER_TYPE Type,
		_In_ ULONG SkipCount
		);
	PROCESS_SIZES_REQUEST GetProcessSizes(
		_In_ HANDLE ProcessId,
		_In_ ULONG EpochExecutionTime
		);
	PIMAGE_DETAILED_REQUEST RequestDetailedImage(
		_In_ HANDLE ProcessId,
		_In_ ULONG EpochExecutionTime,
		_In_ ULONG ImageIndex,
		_In_ ULONG MaxStackSize
		);
	GLOBAL_SIZES GetGlobalSizes(
		VOID
		);
	BOOLEAN DeleteFilter(
		_In_ FILTER_INFO Filter
		);
};