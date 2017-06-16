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

bool KExploreHelper::ExtractResourceToFile(HMODULE hModule, PCWSTR resourceName, PCWSTR targetFile) {
    auto hResource = ::FindResource(hModule, resourceName, L"BIN");
    if(hResource == nullptr)
        return false;

    auto hGlobal = ::LoadResource(hModule, hResource);
    auto size = ::SizeofResource(hModule, hResource);
    auto data = ::LockResource(hGlobal);
    if(data == nullptr)
        return false;

    HANDLE hFile = ::CreateFile(targetFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if(hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD written;
    BOOL success = ::WriteFile(hFile, data, size, &written, nullptr);
    ::CloseHandle(hFile);

    return success ? true : false;

}

bool KExploreHelper::LoadDriver(PCWSTR name) {
    auto hScm = ::OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if(hScm == nullptr)
        return false;

    auto hService = ::OpenService(hScm, name, SERVICE_ALL_ACCESS);
    if(hService == nullptr) {
        ::CloseServiceHandle(hScm);
        return false;
    }

    BOOL success = ::StartService(hService, 0, nullptr);
    ::CloseServiceHandle(hService);
    ::CloseServiceHandle(hScm);

    return success ? true : false;
}

bool KExploreHelper::InstallDriver(PCWSTR name, PCWSTR sysFilePath) {
    auto hScm = ::OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if(hScm == nullptr)
        return false;
    auto hService = ::CreateService(hScm, name, name, SERVICE_ALL_ACCESS, 
        SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, sysFilePath,
        nullptr, nullptr, nullptr, nullptr, nullptr);
    
    bool ok = hService != nullptr;
    
    ::CloseServiceHandle(hService);
    ::CloseServiceHandle(hScm);

    return ok;
}

