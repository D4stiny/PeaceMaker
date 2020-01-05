#pragma once
#include "ntdef.h"

#define RCAST reinterpret_cast
#define SCAST static_cast
#define MAX_PATH 260

#define DBGPRINT(msg, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, msg"\n", __VA_ARGS__)

void* __cdecl operator new(size_t size, POOL_TYPE pool, ULONG tag = 0);
void __cdecl operator delete(void* p, unsigned __int64);

#define FlagOn(_F,_SF) ((_F) & (_SF))

#define FILTER_ALTITUDE L"321410"

PPEB NTAPI PsGetProcessPeb(IN PEPROCESS Process);

PRUNTIME_FUNCTION RtlLookupFunctionEntry(_In_ PVOID ControlPc, _Out_ PVOID* ImageBase, _Inout_opt_ UNWIND_HISTORY_TABLE* HistoryTable);
PEXCEPTION_ROUTINE RtlVirtualUnwind(DWORD HandlerType, DWORD64 ImageBase, DWORD64 ControlPc, PRUNTIME_FUNCTION FunctionEntry, PCONTEXT ContextRecord, PVOID* HandlerData, PDWORD64 EstablisherFrame, PKNONVOLATILE_CONTEXT_POINTERS ContextPointers);