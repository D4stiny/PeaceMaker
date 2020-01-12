#pragma once
#include "common.h"

#define NT_DEVICE_NAME      L"\\Device\\PeaceMaker"
#define DOS_DEVICE_NAME     L"\\DosDevices\\PeaceMaker"

class IOCTLCommunication
{
	PDRIVER_OBJECT DriverObject;

	NTSTATUS InitializeDriverIOCTL(VOID);
	NTSTATUS UninitializeDriverIOCTL(VOID);

	static NTSTATUS IOCTLCreateClose(
		_In_ PDEVICE_OBJECT DeviceObject,
		_In_ PIRP Irp
		);
	static NTSTATUS IOCTLDeviceControl(
		_In_ PDEVICE_OBJECT DeviceObject,
		_In_ PIRP Irp
		);

public:
	IOCTLCommunication(
		_In_ PDRIVER_OBJECT DriverObject
		);
	~IOCTLCommunication(VOID);
};