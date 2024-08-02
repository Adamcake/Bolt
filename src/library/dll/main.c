#include <Windows.h>
#include "common.h"
#include "dll_inject.h"

typedef HMODULE(__stdcall* LOADLIBRARYW)(LPCWSTR);
typedef HMODULE(__stdcall* GETMODULEHANDLEW)(LPCWSTR);
typedef FARPROC(__stdcall* GETPROCADDRESS)(HMODULE, LPCSTR);
typedef BOOL(__stdcall* VIRTUALPROTECT)(LPVOID, SIZE_T, DWORD, PDWORD);

static void resolve_imports(HMODULE, PIMAGE_IMPORT_DESCRIPTOR, HMODULE luajit, GETMODULEHANDLEW, GETPROCADDRESS, LOADLIBRARYW, VIRTUALPROTECT);

// this function is invoked via CreateRemoteThread by the stub dll (see dll_inject).
// the plugin dll is injected into the suspended game process during a CreateProcessW hook, then it
// calls this function, awaits its return, and finally resumes the process.
// the import table is not resolved at this point. we rely on kernel32.dll having a stable address
// so we can assume GetModuleHandle and GetProcAddress will be the same as in any other process, so
// we can not only resolve our own IAT but also place hooks in the game's IAT.
// note that it is not safe to use any libc function from this function. doing so will probably crash.
DWORD __stdcall BOLT_STUB_ENTRYNAME(struct PluginInjectParams* data) {
    LOADLIBRARYW pLoadLibraryW = (LOADLIBRARYW)data->pGetProcAddress(data->kernel32, "LoadLibraryW");
    VIRTUALPROTECT pVirtualProtect = (VIRTUALPROTECT)data->pGetProcAddress(data->kernel32, "VirtualProtect");

    // before doing anything else, we need to sort out our import tables, starting with lua's.
    // LoadLibrary would normally do this for us, but this DLL gets loaded by a manual map injector, not LoadLibrary.
    resolve_imports(data->luajit, data->luajit_import_directory, data->luajit, data->pGetModuleHandleW, data->pGetProcAddress, pLoadLibraryW, pVirtualProtect);
    resolve_imports(data->plugin, data->plugin_import_directory, data->luajit, data->pGetModuleHandleW, data->pGetProcAddress, pLoadLibraryW, pVirtualProtect);

    return 0;
}

static void resolve_imports(HMODULE image_base, PIMAGE_IMPORT_DESCRIPTOR import_, HMODULE luajit, GETMODULEHANDLEW pGetModuleHandleW, GETPROCADDRESS pGetProcAddress, LOADLIBRARYW pLoadLibraryW, VIRTUALPROTECT pVirtualProtect) {
    // 4096 appears to be the maximum allowed name of a DLL or exported function, though they don't expose a constant for it
    WCHAR wname_buffer[4096];

    PIMAGE_IMPORT_DESCRIPTOR import = import_;
	while (import->Characteristics) {
		PIMAGE_THUNK_DATA orig_first_thunk = (PIMAGE_THUNK_DATA)((LPBYTE)image_base + import->OriginalFirstThunk);
		PIMAGE_THUNK_DATA first_thunk = (PIMAGE_THUNK_DATA)((LPBYTE)image_base + import->FirstThunk);
		LPCSTR dll_name = (LPCSTR)((LPBYTE)image_base + import->Name);
        HMODULE hmodule;
        if (bolt_cmp(dll_name, "lua51.dll", 0)) {
            hmodule = luajit;
        } else {
            size_t i = 0;
            while (1) {
                // this is not a great way of converting LPCSTR to LPCWSTR, but it's safe in this case
                // because DLL names are ASCII. generally you should use MultiByteToWideChar().
                if (!(wname_buffer[i] = (WCHAR)dll_name[i])) break;
                i += 1;
            }
		    hmodule = pGetModuleHandleW(wname_buffer);
            if (!hmodule) hmodule = pLoadLibraryW(wname_buffer);
        }

		while (orig_first_thunk->u1.AddressOfData) {
            DWORD oldp;
            pVirtualProtect((LPVOID)(&first_thunk->u1.Function), sizeof(first_thunk->u1.Function), PAGE_READWRITE, &oldp);
			if (orig_first_thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
				// import by ordinal
				first_thunk->u1.Function = (ULONGLONG)pGetProcAddress(hmodule, (LPCSTR)(orig_first_thunk->u1.Ordinal & 0xFFFF));
			} else {
				// import by name
				PIMAGE_IMPORT_BY_NAME pibn = (PIMAGE_IMPORT_BY_NAME)((LPBYTE)image_base + orig_first_thunk->u1.AddressOfData);
				first_thunk->u1.Function = (ULONGLONG)pGetProcAddress(hmodule, (LPCSTR)pibn->Name);
			}
            pVirtualProtect((LPVOID)(&first_thunk->u1.Function), sizeof(first_thunk->u1.Function), oldp, &oldp);
			orig_first_thunk += 1;
            first_thunk += 1;
		}
		import += 1;
	}
}
