#include "pch.h"
#include "KExploreClient.h"
#include "KExplore.h"

#define DRIVER_PREFIX "KExplore: "

KernelFunctions g_KernelFunctions;

// prototypes

void KExploreUnload(PDRIVER_OBJECT);
NTSTATUS KExploreDeviceControl(PDEVICE_OBJECT, PIRP);

// DriverEntry

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {
    KdPrint((DRIVER_PREFIX "DriverEntry\n"));

    NTSTATUS status = STATUS_SUCCESS;

    UNICODE_STRING deviceName;
    UNICODE_STRING win32Name;

    RtlInitUnicodeString(&deviceName, L"\\Device\\KExplore");
    RtlInitUnicodeString(&win32Name, L"\\??\\KExplore");

    PDEVICE_OBJECT device;
    status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &device);
    if (!NT_SUCCESS(status)) {
        KdPrint((DRIVER_PREFIX "Failed to create device object, status=%!STATUS!\n"));
        return status;
    }

    status = IoCreateSymbolicLink(&win32Name, &deviceName);
    if (!NT_SUCCESS(status)) {
        KdPrint((DRIVER_PREFIX "Failed to create symbolic link, status=%!STATUS!\n"));
        IoDeleteDevice(device);
        return status;
    }

    DriverObject->DriverUnload = KExploreUnload;

    DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = [](PDEVICE_OBJECT, PIRP Irp) {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, 0);
        return STATUS_SUCCESS;
    };

    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = KExploreDeviceControl;

    return status;

}

void KExploreUnload(PDRIVER_OBJECT DriverObject) {
    KdPrint((DRIVER_PREFIX "Unload\n"));

    UNICODE_STRING win32Name;
    RtlInitUnicodeString(&win32Name, L"\\??\\KExplore");
    IoDeleteSymbolicLink(&win32Name);
    IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS KExploreDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
    auto stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG_PTR len = 0;

    switch (stack->Parameters.DeviceIoControl.IoControlCode) {
    case KEXPLORE_IOCTL_GET_EXPORTED_NAME: {
        UNICODE_STRING name;
        PCWSTR exportName = static_cast<PCWSTR>(Irp->AssociatedIrp.SystemBuffer);
        RtlInitUnicodeString(&name, exportName);
        void* address = MmGetSystemRoutineAddress(&name);
        *(void**)exportName = address;
        len = sizeof(void*);
        break;
    }

    case KEXPLORE_IOCTL_ENUM_JOBS: {
        if (g_KernelFunctions.PspGetNextJob == nullptr) {
            g_KernelFunctions.PspGetNextJob = static_cast<FPspGetNextJob>(*(void**)(Irp->AssociatedIrp.SystemBuffer));
        }
        auto PspGetNextJob = g_KernelFunctions.PspGetNextJob;
        if (PspGetNextJob == nullptr) {
            status = STATUS_INVALID_ADDRESS;
            KdPrint((DRIVER_PREFIX "Missing PspGetNextJob function\n"));
            break;
        }

        auto size = stack->Parameters.DeviceIoControl.OutputBufferLength;
        auto output = static_cast<void**>(Irp->AssociatedIrp.SystemBuffer);

        int count = 0;
        for (auto job = PspGetNextJob(nullptr); job; job = PspGetNextJob(job)) {
            if (size >= sizeof(job)) {
                output[count++] = job;
                size -= sizeof(job);
            }
        }

        len = count * sizeof(PVOID);
        if (count * sizeof(PVOID) > size)
            status = STATUS_MORE_ENTRIES;
        break;
    }

    case KEXPLORE_IOCTL_OPEN_OBJECT_HANDLE: {
        HANDLE hObject = nullptr;
        auto data = static_cast<OpenHandleData*>(Irp->AssociatedIrp.SystemBuffer);
        status = ObOpenObjectByPointer(data->Object, 0, nullptr, data->AccessMask, nullptr, KernelMode, &hObject);
        if (NT_SUCCESS(status)) {
            *(HANDLE*)data = hObject;
            len = sizeof(HANDLE);
        }
        break;
    }

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = len;
    IoCompleteRequest(Irp, 0);
    return status;

}
