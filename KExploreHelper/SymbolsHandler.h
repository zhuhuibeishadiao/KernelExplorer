#pragma once

#pragma warning(disable:4091)
#include <DbgHelp.h>
#pragma warning(default:4091)

class SymbolInfo {
public:
    SymbolInfo();
    ~SymbolInfo();

    operator PSYMBOL_INFO() const  {
        return m_Symbol;
    }

    SYMBOL_INFO* GetSymbolInfo() const {
        return m_Symbol;
    }

private:
    SYMBOL_INFO* m_Symbol;
};

class SymbolsHandler
{
public:
    SymbolsHandler(PCSTR searchPath = nullptr, DWORD symOptions = SYMOPT_ALLOW_ZERO_ADDRESS | SYMOPT_CASE_INSENSITIVE | SYMOPT_ALLOW_ABSOLUTE_SYMBOLS);
    ~SymbolsHandler();
    ULONG64 LoadSymbolsForModule(PCSTR moduleName);
    std::unique_ptr<SymbolInfo> GetSymbolFromName(PCSTR name);

private:
    HANDLE m_hProcess;
};

