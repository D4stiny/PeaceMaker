#include "common.h"
#include "shared.h"

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

NTSTATUS NtQueryInformationProcess(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength)
{
	UNICODE_STRING funcName;
	typedef NTSTATUS(NTAPI* NtQueryInformationProcess_t)(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength);
	static NtQueryInformationProcess_t fNtQueryInformationProcess = NULL;

	if (fNtQueryInformationProcess == NULL)
	{
		RtlInitUnicodeString(&funcName, L"ZwQueryInformationProcess");
		fNtQueryInformationProcess = RCAST<NtQueryInformationProcess_t>(MmGetSystemRoutineAddress(&funcName));
	}

	return fNtQueryInformationProcess(ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength);
}
