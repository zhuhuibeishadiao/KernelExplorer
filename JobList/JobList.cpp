// JobList.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "..\KExploreHelper\KExploreHelper.h"
#include "..\KExploreHelper\SymbolsHandler.h"
#include "..\KExplore\KExploreClient.h"
#include <winternl.h>
#include "resource.h"

#pragma comment(lib, "ntdll")

enum ObjectInformationClass {
    ObjectNameInformation = 1,
};


enum class Options {
    None = 0,
    NamedObjects = 1
};

int Error(const char* message) {
    printf("%s (%d)\n", message, ::GetLastError());
    return 1;
}

void PrintHeader() {
    printf("JobList v0.1 (C)2017 by Pavel Yosifovich\n\n");
}

void PrintJob(void* job, HANDLE hJob, Options options) {
    auto blist = std::make_unique<BYTE[]>(256);
    auto list = reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(blist.get());
    DWORD len;
    BOOL processListOk = ::QueryInformationJobObject(hJob, JobObjectBasicProcessIdList, list, 256, &len);
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION basicInfo;
    BOOL basicInfoOk = ::QueryInformationJobObject(hJob, JobObjectBasicAccountingInformation, &basicInfo, sizeof(basicInfo), &len);

    auto bstr = std::make_unique<BYTE[]>(512);
    auto str = reinterpret_cast<UNICODE_STRING*>(bstr.get());  
    ::NtQueryObject(hJob, (OBJECT_INFORMATION_CLASS) ObjectNameInformation, str, 512, nullptr);
    if(((int)options & (int)Options::NamedObjects) == 0 || (str->Length > 0)) {
        printf("0x%p %-72ws", job, str->Buffer == nullptr ? L"" : str->Buffer);
        if(basicInfoOk)
            printf("%d/%d/%d ", basicInfo.TotalProcesses, basicInfo.ActiveProcesses, basicInfo.TotalTerminatedProcesses);

        if(processListOk) {
            printf("( ");
            for(DWORD i = 0; i < list->NumberOfProcessIdsInList; i++)
                printf("%d ", static_cast<int>(list->ProcessIdList[i]));
            printf(")");
        }
        printf("\n");
    }
}

void PrintTableHeader() {
    printf("%-20s%-60s%s\n", 
        "Job address", "Name", "Processes: total/active/terminated (list)"); 
    printf("%s\n\n", std::string(121, '-').c_str());
}

Options ParseCommandLineOptions(int argc, char* argv[]) {
    int options = (int)Options::None;

    for(int i = 1; i < argc; i++) {
        if(_stricmp(argv[i], "-named") == 0)
            options |= (int)Options::NamedObjects;
    }

    return (Options)options;
}

bool Initialize() {
    // load driver
    WCHAR driverName[] = L"KExplore";

    WCHAR path[MAX_PATH];
    ::GetModuleFileName(nullptr, path, MAX_PATH);
    std::wstring exePath(path), fullPath;
    HMODULE hInstance = ::GetModuleHandle(exePath.c_str());
    *(1 + wcsrchr(path, L'\\')) = UNICODE_NULL;

    if(!KExploreHelper::LoadDriver(driverName)) {
        // extract and install
        fullPath = path;
        fullPath += L"KExplore.sys";
        if(!KExploreHelper::ExtractResourceToFile(hInstance, MAKEINTRESOURCE(IDR_DRIVER), fullPath.c_str())) {
            printf("Failed to extract driver binary\n");
            return false;
        }

        if(!KExploreHelper::InstallDriver(driverName, fullPath.c_str())) {
            printf("Failed to install driver\n");
            return false;
        }

        if(!KExploreHelper::LoadDriver(driverName)) {
            printf("Failed to start driver\n");
            return false;
        }
    }

    // extract DLLs
    fullPath = path;
    fullPath += L"DbgHelp.dll";
    if(KExploreHelper::ExtractResourceToFile(hInstance, MAKEINTRESOURCE(IDR_DBGHELP), fullPath.c_str())) {
        fullPath = path;
        fullPath += L"SymSrv.dll";
        if(!KExploreHelper::ExtractResourceToFile(hInstance, MAKEINTRESOURCE(IDR_SYMSRV), fullPath.c_str())) {
            printf("Failed to extract DLLs\n");
            return false;
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
    auto hDevice = KExploreHelper::OpenDriverHandle();
    if (hDevice == INVALID_HANDLE_VALUE) {
        if(!Initialize())
            return 1;

        // try again by running again (so that the DLLs would load early enough)

        PROCESS_INFORMATION pi;
        STARTUPINFO si = { sizeof(si) };
        CreateProcess(nullptr, ::GetCommandLine(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
        return 0;
    }

    PrintHeader();
    Options options = ParseCommandLineOptions(argc, argv);

    SymbolsHandler handler;
    auto address = handler.LoadSymbolsForModule("%systemroot%\\system32\\ntoskrnl.exe");
    auto PspGetNextJobSymbol = handler.GetSymbolFromName("PspGetNextJob");
    if (PspGetNextJobSymbol == nullptr)
        return Error("No symbols have been found or SymSrv.dll missing. Please check _NT_SYMBOL_PATH environment variable");

    // calculate the exact function address

    void* PspGetNextJob = (void*)((ULONG_PTR)KExploreHelper::GetKernelBaseAddress() + PspGetNextJobSymbol->GetSymbolInfo()->Address - address);

    DWORD returned;
    void* jobs[1024];
    if (::DeviceIoControl(hDevice, KEXPLORE_IOCTL_ENUM_JOBS, &PspGetNextJob, sizeof(PspGetNextJob), jobs, sizeof(jobs), &returned, nullptr)) {
        int countJobs = returned / sizeof(jobs[0]);
        printf("Found %d jobs.\n", countJobs);

        PrintTableHeader();

        OpenHandleData data;
        data.AccessMask = JOB_OBJECT_QUERY;

        for (int i = 0; i < countJobs; i++) {
            data.Object = jobs[i];
            HANDLE hJob;
            if (::DeviceIoControl(hDevice, KEXPLORE_IOCTL_OPEN_OBJECT_HANDLE, &data, sizeof(data), &hJob, sizeof(hJob), &returned, nullptr)) {
                PrintJob(jobs[i], hJob, options);

                ::CloseHandle(hJob);
            }
            else {
                printf("Error opening job %i: %d\n", i, GetLastError());
            }
        }


        ::CloseHandle(hDevice);

    }
    return 0;
}
