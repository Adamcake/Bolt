#include "stub_inject.hxx"

//typedef HANDLE(__stdcall* CREATEFILEW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
//typedef FARPROC(__stdcall* CLOSEHANDLE)(HANDLE);
//typedef BOOL(__stdcall* WRITEFILE)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef BOOL(__stdcall* CREATEPROCESSW)(LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION);
typedef BOOL(__stdcall* VIRTUALPROTECT)(LPVOID, SIZE_T, DWORD, PDWORD);

//static CREATEFILEW real_CreateFileW;
//static CLOSEHANDLE real_CloseHandle;
//static WRITEFILE real_WriteFile;

static CREATEPROCESSW real_CreateProcessW;
BOOL hook_CreateProcessW(LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION);

// true if match, false if not, for case-insensitive mode a2 must be all lowercase
BOOL bolt_cmp(const char* a1, const char* a2, BOOL case_sensitive) {
    while (1) {
        if (*a1 != *a2) {
            if (case_sensitive || (*a2 < 'a' || *a2 > 'z')) return 0;
            if (*a1 != *a2 - ('a' - 'A')) return 0;
        }
        if (!*a1) return 1;
        a1 += 1;
        a2 += 1;
    }
}

// this function is invoked via CreateRemoteThread by the manual map injector (see stub_inject)
// after it injects it into the process.
// it is not safe to call any imported functions from inside the stub dll, since the import table is
// never resolved. we rely instead on the fact that kernel32.dll has a stable address, so the host
// process passes us its function pointers for GetModuleHandleW and GetProcAddress. using those, we
// can find the game's imported modules and use them for ourselves.
DWORD __stdcall BOLT_STUB_ENTRYNAME(struct StubInjectParams* data) {
    VIRTUALPROTECT pVirtualProtect = (VIRTUALPROTECT)data->pGetProcAddress(data->kernel32, "VirtualProtect");
    //real_CreateFileW = (CREATEFILEW)data->pGetProcAddress(data->kernel32, "CreateFileW");
    //real_CloseHandle = (CLOSEHANDLE)data->pGetProcAddress(data->kernel32, "CloseHandle");
    //real_WriteFile = (WRITEFILE)data->pGetProcAddress(data->kernel32, "WriteFile");

    HMODULE main_module = data->pGetModuleHandleW(NULL);
    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)main_module;
    PIMAGE_NT_HEADERS nt_headers = (PIMAGE_NT_HEADERS)((LPBYTE)main_module + dos_header->e_lfanew);
    IMAGE_DATA_DIRECTORY import_directory = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

    for (PIMAGE_IMPORT_DESCRIPTOR import_descriptor = (PIMAGE_IMPORT_DESCRIPTOR)((LPBYTE)main_module + import_directory.VirtualAddress); import_descriptor->Name; import_descriptor += 1) {
        if (!bolt_cmp((LPBYTE)main_module + import_descriptor->Name, "kernel32.dll", 0)) continue;
        PIMAGE_THUNK_DATA lookup_table = (PIMAGE_THUNK_DATA)((LPBYTE)main_module + import_descriptor->OriginalFirstThunk);
        PIMAGE_THUNK_DATA address_table = (PIMAGE_THUNK_DATA)((LPBYTE)main_module + import_descriptor->FirstThunk);
        while (lookup_table->u1.AddressOfData) {
            if((lookup_table->u1.AddressOfData & IMAGE_ORDINAL_FLAG) == 0) {
                PIMAGE_IMPORT_BY_NAME fname = (PIMAGE_IMPORT_BY_NAME)((LPBYTE)main_module + lookup_table->u1.AddressOfData);
                if (bolt_cmp(fname->Name, "CreateProcessW", 1)) {
                    DWORD oldp;
                    pVirtualProtect((LPVOID)(&address_table->u1.Function), 8, PAGE_READWRITE, &oldp);
                    real_CreateProcessW = (CREATEPROCESSW)address_table->u1.Function;
                    address_table->u1.Function = (ULONGLONG)hook_CreateProcessW;
                    pVirtualProtect((LPVOID)(&address_table->u1.Function), 8, oldp, &oldp);
                }
            }
            lookup_table += 1;
            address_table += 1;
        }
    }

    return 0;
}

BOOL hook_CreateProcessW(
    LPCWSTR               lpApplicationName,
    LPWSTR                lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL                  bInheritHandles,
    DWORD                 dwCreationFlags,
    LPVOID                lpEnvironment,
    LPCWSTR               lpCurrentDirectory,
    LPSTARTUPINFOW        lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation
) {
    // CreateProcessW hook, check if we're starting rs2client and if so, inject bolt-plugin.dll
    return real_CreateProcessW(
        lpApplicationName,
        lpCommandLine,
        lpProcessAttributes,
        lpThreadAttributes,
        bInheritHandles,
        dwCreationFlags,
        lpEnvironment,
        lpCurrentDirectory,
        lpStartupInfo,
        lpProcessInformation
    );
}
