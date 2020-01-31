#pragma once
#include "common.h"
#include "AlertQueue.h"
#include "StackWalker.h"
#include "shared.h"

typedef class DetectionLogic
{
	PALERT_QUEUE alerts;
	StackWalker resolver;

	VOID PushStackViolationAlert (
		_In_ DETECTION_SOURCE DetectionSource,
		_In_ PVOID ViolatingAddress,
		_In_ HANDLE SourceProcessId,
		_In_ PUNICODE_STRING SourcePath,
		_In_ PUNICODE_STRING TargetPath,
		_In_ STACK_RETURN_INFO StackHistory[],
		_In_ ULONG StackHistorySize
		);

public:
	DetectionLogic();
	~DetectionLogic();

	PALERT_QUEUE GetAlertQueue (
		VOID
		);

	VOID AuditUserStackWalk (
		_In_ DETECTION_SOURCE DetectionSource,
		_In_ HANDLE SourceProcessId,
		_In_ PUNICODE_STRING SourcePath,
		_In_ PUNICODE_STRING TargetPath,
		_In_ STACK_RETURN_INFO StackHistory[],
		_In_ ULONG StackHistorySize
		);

	VOID AuditUserPointer (
		_In_ DETECTION_SOURCE DetectionSource,
		_In_ PVOID UserPtr,
		_In_ HANDLE SourceProcessId,
		_In_ PUNICODE_STRING SourcePath,
		_In_ PUNICODE_STRING TargetPath,
		_In_ STACK_RETURN_INFO StackHistory[],
		_In_ ULONG StackHistorySize
		);

	VOID AuditCallerProcessId (
		_In_ DETECTION_SOURCE DetectionSource,
		_In_ HANDLE CallerProcessId,
		_In_ HANDLE TargetProcessId,
		_In_ PUNICODE_STRING SourcePath,
		_In_ PUNICODE_STRING TargetPath,
		_In_ STACK_RETURN_INFO StackHistory[],
		_In_ ULONG StackHistorySize
		);
} DETECTION_LOGIC, *PDETECTION_LOGIC;

#define ALERT_QUEUE_TAG 'qAmP'
#define STACK_VIOLATION_TAG 'vSmP'