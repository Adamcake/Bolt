#include <Windows.h>

struct PluginInjectParams {
    HMODULE kernel32;
    HMODULE(__stdcall* pGetModuleHandleW)(LPCWSTR);
    FARPROC(__stdcall* pGetProcAddress)(HMODULE, LPCSTR);
    HMODULE plugin;
    HMODULE luajit;
#if defined(_WIN64)
    PIMAGE_RUNTIME_FUNCTION_ENTRY luajit_exception_directory;
    DWORD luajit_rtl_entrycount;
#endif
    PIMAGE_IMPORT_DESCRIPTOR plugin_import_directory;
    PIMAGE_IMPORT_DESCRIPTOR luajit_import_directory;
    PIMAGE_EXPORT_DIRECTORY luajit_export_directory;
};

void InjectPluginDll(HANDLE process, HMODULE kernel32, HMODULE(__stdcall* pGetModuleHandleW)(LPCWSTR), FARPROC(__stdcall* pGetProcAddress)(HMODULE, LPCSTR));
