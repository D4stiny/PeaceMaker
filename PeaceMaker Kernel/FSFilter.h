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
#include "ImageHistoryFilter.h"

typedef class FSBlockingFilter
{
	static FLT_PREOP_CALLBACK_STATUS
	HandlePreCreateOperation (
		_Inout_ PFLT_CALLBACK_DATA Data,
		_In_ PCFLT_RELATED_OBJECTS FltObjects,
		_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
		);

	static FLT_PREOP_CALLBACK_STATUS
	HandlePreWriteOperation (
		_Inout_ PFLT_CALLBACK_DATA Data,
		_In_ PCFLT_RELATED_OBJECTS FltObjects,
		_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
		);

	static FLT_PREOP_CALLBACK_STATUS
	HandlePreSetInfoOperation (
		_Inout_ PFLT_CALLBACK_DATA Data,
		_In_ PCFLT_RELATED_OBJECTS FltObjects,
		_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
		);

	static NTSTATUS
	HandleInstanceSetup (
		_In_ PCFLT_RELATED_OBJECTS FltObjects,
		_In_ FLT_INSTANCE_SETUP_FLAGS Flags,
		_In_ DEVICE_TYPE VolumeDeviceType,
		_In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
		);

	static NTSTATUS
	HandleInstanceQueryTeardown (
		_In_ PCFLT_RELATED_OBJECTS FltObjects,
		_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
		);

	static VOID
	HandleInstanceTeardownStart (
		_In_ PCFLT_RELATED_OBJECTS FltObjects,
		_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
		);

	static VOID
	HandleInstanceTeardownComplete (
		_In_ PCFLT_RELATED_OBJECTS FltObjects,
		_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
		);

	//
	// Class callbacks for necessary filesystem operations.
	//
	static constexpr FLT_OPERATION_REGISTRATION Callbacks[] = {

		{ IRP_MJ_CREATE,
		  0,
		  FSBlockingFilter::HandlePreCreateOperation,
		  NULL },

		{ IRP_MJ_WRITE,
		  0,
		  FSBlockingFilter::HandlePreWriteOperation,
		  NULL },

		{ IRP_MJ_SET_INFORMATION,
		  FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,
		  FSBlockingFilter::HandlePreSetInfoOperation,
		  NULL },

		{ IRP_MJ_OPERATION_END }
	};

	//
	// The registration context for the mini-filter.
	//
	static FLT_REGISTRATION FilterRegistration;

	//
	// Contains strings to block various filesystem operations.
	//
	static PSTRING_FILTERS FileStringFilters;

	static STACK_WALKER walker;
	static PDETECTION_LOGIC detector;
public:
	FSBlockingFilter (
		_In_ PDRIVER_OBJECT DriverObject,
		_In_ PUNICODE_STRING RegistryPath,
		_In_ PFLT_FILTER_UNLOAD_CALLBACK UnloadRoutine,
		_In_ PDETECTION_LOGIC Detector,
		_Out_ NTSTATUS* InitializeStatus,
		_Out_ PFLT_FILTER* FilterHandle
		);
	~FSBlockingFilter();

	static PSTRING_FILTERS GetStringFilters();
	
} FS_BLOCKING_FILTER, *PFS_BLOCKING_FILTER;

#define STRING_FILE_FILTERS_TAG 'fFmP'