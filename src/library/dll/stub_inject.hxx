#include <Windows.h>

struct StubInjectParams {
    HMODULE kernel32;
    HMODULE(__stdcall* pGetModuleHandleW)(LPCWSTR);
    FARPROC(__stdcall* pGetProcAddress)(HMODULE, LPCSTR);
};

void InjectStub(HANDLE process);
