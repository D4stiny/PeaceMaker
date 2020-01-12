#pragma once
#include "common.h"
#include "DetectionLogic.h"
#include "ImageHistoryFilter.h"

#define NT_DEVICE_NAME      L"\\Device\\PeaceMaker"
#define DOS_DEVICE_NAME     L"\\DosDevices\\PeaceMaker"

class IOCTLCommunication
{
	static PDRIVER_OBJECT DriverObject;
	static PDETECTION_LOGIC Detector;
	static PIMAGE_HISTORY_FILTER ImageProcessFilter;


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
		_In_ PDRIVER_OBJECT DriverObject,
		_Inout_ NTSTATUS* InitializeStatus
		);
	~IOCTLCommunication(VOID);
};

#define DETECTION_LOGIC_TAG 'lDmP'
#define IMAGE_HISTORY_FILTER_TAG 'fImP'