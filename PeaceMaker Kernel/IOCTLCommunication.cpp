#include "IOCTLCommunication.h"

/**
	Construct the IOCTLCommunication class by initializing the driver object.
	@param DriverObject - The driver's object.
*/
IOCTLCommunication::IOCTLCommunication (
	_In_ PDRIVER_OBJECT DriverObject
	)
{
	this->DriverObject = DriverObject;
	InitializeDriverIOCTL();
}

/**
	Deconstruct the IOCTLCommunication class.
*/
IOCTLCommunication::~IOCTLCommunication	(
	VOID
	)
{
	this->DriverObject = DriverObject;
	InitializeDriverIOCTL();
}

/**
	Handle basic create / close of device, always return success with no change.
	@param DeviceObject - The driver's device object.
	@param Irp - The current IRP.
*/
NTSTATUS
IOCTLCommunication::IOCTLCreateClose (
	_In_ PDEVICE_OBJECT DeviceObject,
	_In_ PIRP Irp
	)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	PAGED_CODE();

	//
	// Just accept everyone for now.
	// TODO: Implement some sort of authentication?
	//
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

/**
	Handle IO controls for communication with the PeaceMaker user-mode interface.
	@param DeviceObject - The driver's device object.
	@param Irp - The current IRP.
*/
NTSTATUS
IOCTLCommunication::IOCTLDeviceControl (
	_In_ PDEVICE_OBJECT DeviceObject,
	_In_ PIRP Irp
	)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	PAGED_CODE();

	//
	// Just accept everyone for now.
	// TODO: Implement various IOCTL codes.
	//
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

/**
	Initialize the driver object to support IOCTL communication.
*/
NTSTATUS
IOCTLCommunication::InitializeDriverIOCTL (
	VOID
	)
{
	NTSTATUS status;
	UNICODE_STRING ioctlDeviceName;
	UNICODE_STRING ioctlDosDevicesName;
	PDEVICE_OBJECT ioctlDevice;

	RtlInitUnicodeString(&ioctlDeviceName, NT_DEVICE_NAME);

	//
	// Create IO Device Object.
	//
	status = IoCreateDevice(DriverObject, NULL, &ioctlDeviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &ioctlDevice);
	if (NT_SUCCESS(status) == FALSE)
	{
		DBGPRINT("IOCTLCommunication!InitializeDriverIOCTL: Failed to create kernel device object with error 0x%X.", status);
		goto Exit;
	}

	//
	// Set the handlers for our IOCTL.
	//
	DriverObject->MajorFunction[IRP_MJ_CREATE] = IOCTLCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = IOCTLCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IOCTLDeviceControl;

	RtlInitUnicodeString(&ioctlDosDevicesName, DOS_DEVICE_NAME);

	status = IoCreateSymbolicLink(&ioctlDosDevicesName, &ioctlDeviceName);
	if (NT_SUCCESS(status) == FALSE)
	{
		DBGPRINT("IOCTLCommunication!InitializeDriverIOCTL: Failed to create symbolic link to device with error 0x%X.", status);
		IoDeleteDevice(ioctlDevice);
		goto Exit;
	}
Exit:
	return status;
}

/**
	Undo everything done in InitializeDriverObject.
	@return The status of uninitialization.
*/
NTSTATUS
IOCTLCommunication::UninitializeDriverIOCTL (
	VOID
	)
{
	PDEVICE_OBJECT deviceObject;
	UNICODE_STRING ioctlDosDevicesName;

	PAGED_CODE();

	deviceObject = DriverObject->DeviceObject;

	//
	// Initialize the unicode string of our DosDevices symlink.
	//
	RtlInitUnicodeString(&ioctlDosDevicesName, DOS_DEVICE_NAME);

	//
	// Delete IOCTL symlink because we're unloading.
	//
	IoDeleteSymbolicLink(&ioctlDosDevicesName);
	if (deviceObject != NULL)
	{
		//
		// Delete the device while unloading.
		//
		IoDeleteDevice(deviceObject);
	}
}