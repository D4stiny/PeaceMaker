#include "common.h"

void* __cdecl operator new(size_t size, POOL_TYPE pool, ULONG tag) {
	return ExAllocatePoolWithTag(pool, size, tag);
}

void __cdecl operator delete(void* p, unsigned __int64) {
	ExFreePool(p);
}

PPEB PsGetProcessPeb(IN PEPROCESS Process)
{
	UNICODE_STRING funcName;
	typedef PPEB(NTAPI* PsGetProcessPeb_t)(PEPROCESS Process);
	static PsGetProcessPeb_t fPsGetProcessPeb = NULL;

	if (fPsGetProcessPeb == NULL)
	{
		RtlInitUnicodeString(&funcName, L"PsGetProcessPeb");
		fPsGetProcessPeb = RCAST<PsGetProcessPeb_t>(MmGetSystemRoutineAddress(&funcName));
	}

	return fPsGetProcessPeb(Process);
}

PRUNTIME_FUNCTION RtlLookupFunctionEntry(PVOID ControlPc, PVOID* ImageBase, UNWIND_HISTORY_TABLE* HistoryTable)
{
	UNICODE_STRING funcName;
	typedef PRUNTIME_FUNCTION(NTAPI* RtlLookupFunctionEntry_t)(PVOID ControlPc, PVOID* ImageBase, UNWIND_HISTORY_TABLE* HistoryTable);
	static RtlLookupFunctionEntry_t fRtlLookupFunctionEntry = NULL;

	if (fRtlLookupFunctionEntry == NULL)
	{
		RtlInitUnicodeString(&funcName, L"RtlLookupFunctionEntry");
		fRtlLookupFunctionEntry = RCAST<RtlLookupFunctionEntry_t>(MmGetSystemRoutineAddress(&funcName));
	}

	return fRtlLookupFunctionEntry(ControlPc, ImageBase, HistoryTable);
}

PEXCEPTION_ROUTINE RtlVirtualUnwind(DWORD HandlerType, DWORD64 ImageBase, DWORD64 ControlPc, PRUNTIME_FUNCTION FunctionEntry, PCONTEXT ContextRecord, PVOID* HandlerData, PDWORD64 EstablisherFrame, PKNONVOLATILE_CONTEXT_POINTERS ContextPointers)
{
	UNICODE_STRING funcName;
	typedef PEXCEPTION_ROUTINE(NTAPI* RtlVirtualUnwind_t)(DWORD HandlerType, DWORD64 ImageBase, DWORD64 ControlPc, PRUNTIME_FUNCTION FunctionEntry, PCONTEXT ContextRecord, PVOID* HandlerData, PDWORD64 EstablisherFrame, PKNONVOLATILE_CONTEXT_POINTERS ContextPointers);
	static RtlVirtualUnwind_t fRtlVirtualUnwind = NULL;

	if (fRtlVirtualUnwind == NULL)
	{
		RtlInitUnicodeString(&funcName, L"RtlVirtualUnwind");
		fRtlVirtualUnwind = RCAST<RtlVirtualUnwind_t>(MmGetSystemRoutineAddress(&funcName));
	}

	return fRtlVirtualUnwind(HandlerType, ImageBase, ControlPc, FunctionEntry, ContextRecord, HandlerData, EstablisherFrame, ContextPointers);
}
