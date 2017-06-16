#include "stdafx.h"
#include "KExploreHelper.h"

void* KExploreHelper::GetKernelBaseAddress() {
    void* kernel;
    DWORD needed;
    if(EnumDeviceDrivers(&kernel, sizeof(kernel), &needed))
        return kernel;
    return nullptr;
}

HANDLE KExploreHelper::OpenDriverHandle(PCWSTR name) {
    return ::CreateFile(
        name == nullptr ? L"\\\\.\\KExplore" : name,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);
}


