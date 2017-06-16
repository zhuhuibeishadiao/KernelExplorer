// JobList.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "..\KExploreHelper\KExploreHelper.h"
#include "..\KExploreHelper\SymbolsHandler.h"
#include "..\KExplore\KExploreClient.h"
#include <winternl.h>

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
        printf("0x%p %-72ws", job, str->Buffer);
        if(basicInfoOk)
            printf("%d/%d/%d ", basicInfo.TotalProcesses, basicInfo.ActiveProcesses, basicInfo.TotalTerminatedProcesses);

        if(processListOk) {
            printf("(");
            for(DWORD i = 0; i < list->NumberOfProcessIdsInList; i++)
                printf("%d ", list->ProcessIdList[i]);
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

int main(int argc, char* argv[]) {
    PrintHeader();

    Options options = ParseCommandLineOptions(argc, argv);

    auto hDevice = KExploreHelper::OpenDriverHandle();
    if (hDevice == INVALID_HANDLE_VALUE)
        return Error("Failed to open driver handle");


    SymbolsHandler handler;
    auto address = handler.LoadSymbolsForModule("%systemroot%\\system32\\ntoskrnl.exe");
    auto PspGetNextJobSymbol = handler.GetSymbolFromName("PspGetNextJob");
    if (PspGetNextJobSymbol == nullptr)
        return Error("No symbols have been found. Please check _NT_SYMBOL_PATH environment variable");

    // calculate the excat function address

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
