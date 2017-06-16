#pragma once

class KExploreHelper {
public:
    static void* GetKernelBaseAddress();
    static bool LoadDriver(PCWSTR name = nullptr);
    static HANDLE OpenDriverHandle(PCWSTR name = nullptr);
    static bool ExtractResourceToFile(PCWSTR resourceName, PCWSTR targetFile);
};
