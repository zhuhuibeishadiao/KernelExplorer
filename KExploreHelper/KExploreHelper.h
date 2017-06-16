#pragma once

class KExploreHelper {
public:
    static void* GetKernelBaseAddress();
    static bool LoadDriver(PCWSTR name = nullptr);
    static bool InstallDriver(PCWSTR name, PCWSTR sysFilePath);
    static HANDLE OpenDriverHandle(PCWSTR name = nullptr);
    static bool ExtractResourceToFile(HMODULE hModule, PCWSTR resourceName, PCWSTR targetFile);
};
