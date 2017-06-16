#include "stdafx.h"
#include "SymbolsHandler.h"

#pragma comment(lib, "dbghelp")

SymbolInfo::SymbolInfo() {
    m_Symbol = static_cast<SYMBOL_INFO*>(calloc(1, sizeof(SYMBOL_INFO) + MAX_SYM_NAME));
    m_Symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    m_Symbol->MaxNameLen = MAX_SYM_NAME;
}

SymbolInfo::~SymbolInfo() {
    ::free(m_Symbol);
}


SymbolsHandler::SymbolsHandler(PCSTR searchPath, DWORD symOptions)
{
    static int _instances = 0;
    m_hProcess = reinterpret_cast<HANDLE>(++_instances);
    ::SymSetOptions(symOptions);
    ::SymInitialize(m_hProcess, searchPath, FALSE);
}


SymbolsHandler::~SymbolsHandler()
{
    ::SymCleanup(m_hProcess);
}

ULONG64 SymbolsHandler::LoadSymbolsForModule(PCSTR moduleName)
{
    auto address = ::SymLoadModuleEx(m_hProcess, nullptr, moduleName, nullptr, 0, 0, nullptr, 0);
    return address;
}

std::unique_ptr<SymbolInfo> SymbolsHandler::GetSymbolFromName(PCSTR name)
{
    auto symbol = std::make_unique<SymbolInfo>();
    if(::SymFromName(m_hProcess, name, symbol->GetSymbolInfo()))
        return symbol;
    return nullptr;
}
