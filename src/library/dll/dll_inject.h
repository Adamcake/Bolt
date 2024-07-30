#include <Windows.h>

struct PluginInjectParams {
    HMODULE kernel32;
    HMODULE(__stdcall* pGetModuleHandleW)(LPCWSTR);
    FARPROC(__stdcall* pGetProcAddress)(HMODULE, LPCSTR);
};

void InjectPluginDll(HANDLE process, HMODULE kernel32, HMODULE(__stdcall* pGetModuleHandleW)(LPCWSTR), FARPROC(__stdcall* pGetProcAddress)(HMODULE, LPCSTR));
