#pragma once
#include "common.h"
#include "DetectionLogic.h"
#include "ImageHistoryFilter.h"
#include "StackWalker.h"

typedef class ThreadFilter
{
	static PDETECTION_LOGIC Detector;	// Detection utility.
	static STACK_WALKER Walker;			// Stack walking utility.

	static PVOID GetThreadStartAddress(
		_In_ HANDLE ThreadId
		);

	static VOID ThreadNotifyRoutine(
		HANDLE ProcessId,
		HANDLE ThreadId,
		BOOLEAN Create
		);
public:
	ThreadFilter(
		_In_ PDETECTION_LOGIC Detector,
		_Inout_ NTSTATUS* InitializeStatus
		);
	~ThreadFilter();


} THREAD_FILTER, *PTHREAD_FILTER;