#include "stub_inject.h"
#include "common.h"
#include <iostream>

#define MAX(A, B) (((A) > (B)) ? (A) : (B))

/// This program is run by the build system on Windows. It manually maps the plugin DLL, then outputs some C code which
/// writes that DLL into a target process, invokes the payload on a remote thread, and blocks until that thread returns.
/// The function generated by this code will be called by the stub DLL during a CreateProcessW hook.
int wmain(int argc, const wchar_t **argv) {
    if (argc != 4) {
        std::cerr << "dll_inject_generator must be called with exactly three arguments: the absolute path to the "
            "plugin dll, the absolute path to lua51.dll, and the absolute path to dll_inject.h." << std::endl;
        return 1;
    }

    HANDLE plugin_file = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (plugin_file == INVALID_HANDLE_VALUE) {
        std::cerr << "dll_inject_generator could not open the dll file: ";
        std::wcerr << argv[1];
        std::cerr << std::endl;
        return 1;
    }
    DWORD plugin_file_size = GetFileSize(plugin_file, NULL);
    PVOID plugin_dll = VirtualAlloc(NULL, plugin_file_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!ReadFile(plugin_file, plugin_dll, plugin_file_size, nullptr, nullptr)) {
        std::cerr << "dll_inject_generator could not read the dll file: " << GetLastError() << std::endl;
        return 1;
    }

    HANDLE luajit_file = CreateFileW(argv[2], GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (luajit_file == INVALID_HANDLE_VALUE) {
        std::cerr << "dll_inject_generator could not open the lua dll file: ";
        std::wcerr << argv[2];
        std::cerr << std::endl;
        return 1;
    }
    DWORD luajit_file_size = GetFileSize(luajit_file, NULL);
    PVOID luajit_dll = VirtualAlloc(NULL, luajit_file_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!ReadFile(luajit_file, luajit_dll, luajit_file_size, nullptr, nullptr)) {
        std::cerr << "dll_inject_generator could not read the lua dll file: " << GetLastError() << std::endl;
        return 1;
    }

    PIMAGE_DOS_HEADER plugin_dos_header = (PIMAGE_DOS_HEADER)plugin_dll;
    PIMAGE_NT_HEADERS plugin_nt_headers = (PIMAGE_NT_HEADERS)((LPBYTE)plugin_dll + plugin_dos_header->e_lfanew);
    PIMAGE_SECTION_HEADER plugin_section_header = (PIMAGE_SECTION_HEADER)(plugin_nt_headers + 1);
    PIMAGE_DOS_HEADER luajit_dos_header = (PIMAGE_DOS_HEADER)luajit_dll;
    PIMAGE_NT_HEADERS luajit_nt_headers = (PIMAGE_NT_HEADERS)((LPBYTE)luajit_dll + luajit_dos_header->e_lfanew);
    PIMAGE_SECTION_HEADER luajit_section_header = (PIMAGE_SECTION_HEADER)(luajit_nt_headers + 1);

    // find a function in the export table with ordinal value 1, so we can hard-code its RVA.
    // this assumes the whole export table of plugin.dll is in one section.
    DWORD entry_point_rva = 0;
    IMAGE_DATA_DIRECTORY plugin_export_virtual_details = plugin_nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    for (size_t i = 0; i < plugin_nt_headers->FileHeader.NumberOfSections; i += 1) {
        if (plugin_section_header[i].VirtualAddress > plugin_export_virtual_details.VirtualAddress) continue;
        if (plugin_section_header[i].VirtualAddress + plugin_section_header[i].Misc.VirtualSize <= plugin_export_virtual_details.VirtualAddress) continue;
        const DWORD export_section_offset = plugin_section_header[i].PointerToRawData - plugin_section_header[i].VirtualAddress;
        PIMAGE_EXPORT_DIRECTORY plugin_exports = (PIMAGE_EXPORT_DIRECTORY)((LPBYTE)plugin_dll + (plugin_export_virtual_details.VirtualAddress + export_section_offset));
        PDWORD export_address_list = (PDWORD)((LPBYTE)plugin_dll + (plugin_exports->AddressOfFunctions + export_section_offset));
        entry_point_rva = export_address_list[BOLT_STUB_ENTRYORDINAL - plugin_exports->Base];
        break;
    }
    if (!entry_point_rva) {
        std::cerr << "dll_inject_generator could not locate entry point" << std::endl;
        return 1;
    }

    // output file header and #include lines
    std::cout << "// This file was created programmatically by Bolt's build system. Don't change this file - change the code that generated it." << std::endl;
    std::cout << "#include <Windows.h>" << std::endl << "#include <stdint.h>" << std::endl << "#include \"";
    std::wcout << argv[3];
    std::cout << "\"" << std::endl;
    // embed dll headers
    std::cout << "static const uint8_t plugin_dll_headers[] = {";
    for (size_t i = 0; i < plugin_nt_headers->OptionalHeader.SizeOfHeaders; i += 1) {
        if (i != 0) std::cout << ",";
        std::cout << (WORD)((LPBYTE)plugin_dll)[i];
    }
    std::cout << "};" << std::endl;
    std::cout << "static const uint8_t luajit_dll_headers[] = {";
    for (size_t i = 0; i < luajit_nt_headers->OptionalHeader.SizeOfHeaders; i += 1) {
        if (i != 0) std::cout << ",";
        std::cout << (WORD)((LPBYTE)luajit_dll)[i];
    }
    std::cout << "};" << std::endl;
    // embed dll sections
    for (size_t i = 0; i < plugin_nt_headers->FileHeader.NumberOfSections; i += 1) {
        std::cout << "static const uint8_t psect" << i << "[] = {";
        LPBYTE section_start = (LPBYTE)plugin_dll + plugin_section_header[i].PointerToRawData;
        for (size_t j = 0; j < plugin_section_header[i].SizeOfRawData; j += 1) {
            if (j != 0) std::cout << ",";
            std::cout << (WORD)section_start[j];
        }
        std::cout << "};" << std::endl;
    }
    for (size_t i = 0; i < luajit_nt_headers->FileHeader.NumberOfSections; i += 1) {
        std::cout << "static const uint8_t luasect" << i << "[] = {";
        LPBYTE section_start = (LPBYTE)luajit_dll + luajit_section_header[i].PointerToRawData;
        for (size_t j = 0; j < luajit_section_header[i].SizeOfRawData; j += 1) {
            if (j != 0) std::cout << ",";
            std::cout << (WORD)section_start[j];
        }
        std::cout << "};" << std::endl;
    }
    // embed the amount of zeroes we'll need (WriteProcessMemory needs actual memory to copy from)
    std::cout << "static const uint8_t zeroes["
        << MAX(plugin_nt_headers->OptionalHeader.SizeOfHeaders, luajit_nt_headers->OptionalHeader.SizeOfHeaders)
        << "] = {0};" << std::endl;

    std::cout << "void InjectPluginDll(HANDLE process, HMODULE kernel32, HMODULE(__stdcall* pGetModuleHandleW)(LPCWSTR), FARPROC(__stdcall* pGetProcAddress)(HMODULE, LPCSTR)) {" << std::endl;
    // we can't call imported functions at this point, so lookup the ones we need from kernel32
#define DEFLOOKUP(RET, FN, ARGS) #RET "(__stdcall* p" #FN ")" ARGS "=(" #RET "(__stdcall*)" ARGS ")pGetProcAddress(kernel32, \"" #FN "\");"
    std::cout <<
        DEFLOOKUP(LPVOID, VirtualAllocEx, "(HANDLE, LPVOID, SIZE_T, DWORD, DWORD)")
        DEFLOOKUP(BOOL, VirtualFreeEx, "(HANDLE, LPVOID, SIZE_T, DWORD)")
        DEFLOOKUP(BOOL, WriteProcessMemory, "(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*)")
        DEFLOOKUP(BOOL, ReadProcessMemory, "(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*)")
        DEFLOOKUP(BOOL, VirtualProtectEx, "(HANDLE, LPVOID, SIZE_T, DWORD, PDWORD)")
        DEFLOOKUP(HANDLE, CreateRemoteThread, "(HANDLE, LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD)")
        DEFLOOKUP(DWORD, WaitForSingleObject, "(HANDLE, DWORD)")
        << std::endl;
#undef DEFLOOKUP
    // allocate memory in the remote process
    std::cout << "LPBYTE dll_plugin = (LPBYTE)pVirtualAllocEx(process, NULL, " << plugin_nt_headers->OptionalHeader.SizeOfImage << ", MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);" << std::endl;
    std::cout << "LPBYTE dll_lua = (LPBYTE)pVirtualAllocEx(process, NULL, " << luajit_nt_headers->OptionalHeader.SizeOfImage << ", MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);" << std::endl;
    // copy DLL headers to the target process
    std::cout << "pWriteProcessMemory(process, dll_plugin, (PVOID)plugin_dll_headers, " << plugin_nt_headers->OptionalHeader.SizeOfHeaders << ", NULL);" << std::endl;
    std::cout << "pWriteProcessMemory(process, dll_lua, (PVOID)luajit_dll_headers, " << luajit_nt_headers->OptionalHeader.SizeOfHeaders << ", NULL);" << std::endl;
    // copy each section to the target process
    for (size_t i = 0; i < plugin_nt_headers->FileHeader.NumberOfSections; i += 1) {
        std::cout << "pWriteProcessMemory(process, (PVOID)(dll_plugin + " << plugin_section_header[i].VirtualAddress << "), (PVOID)psect" << i << ", " << plugin_section_header[i].SizeOfRawData << ", NULL);" << std::endl;
    }
    for (size_t i = 0; i < luajit_nt_headers->FileHeader.NumberOfSections; i += 1) {
        std::cout << "pWriteProcessMemory(process, (PVOID)(dll_lua + " << luajit_section_header[i].VirtualAddress << "), (PVOID)luasect" << i << ", " << luajit_section_header[i].SizeOfRawData << ", NULL);" << std::endl;
    }
    // fix relocations
    std::cout <<
        "ptrdiff_t reloc_delta = (ptrdiff_t)(dll_plugin - " << plugin_nt_headers->OptionalHeader.ImageBase << ");"
        "PIMAGE_BASE_RELOCATION remote_reloc_address = (PIMAGE_BASE_RELOCATION)(dll_plugin + " << plugin_nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress << ");"
        "WORD offset;"
        "LPBYTE ptr;"
        "while (1) {"
          "IMAGE_BASE_RELOCATION remote_reloc;"
          "pReadProcessMemory(process, (LPCVOID)remote_reloc_address, (LPVOID)&remote_reloc, sizeof(remote_reloc), NULL);"
          "if (!remote_reloc.VirtualAddress) break;"
          "if (remote_reloc.SizeOfBlock > sizeof(remote_reloc)) {"
            "PWORD list = (PWORD)(remote_reloc_address + 1);"
            "for (size_t i = 0; i < (remote_reloc.SizeOfBlock - sizeof(remote_reloc)) / sizeof(WORD); i += 1) {"
              "pReadProcessMemory(process, (LPCVOID)(list + i), (LPVOID)&offset, sizeof(offset), NULL);"
              "if (!offset) continue;"
              "LPVOID remote_ptr_address = (LPVOID)(dll_plugin + remote_reloc.VirtualAddress + (offset & 0xFFF));"
              "pReadProcessMemory(process, (LPCVOID)remote_ptr_address, (LPVOID)&ptr, sizeof(ptr), NULL);"
              "ptr += reloc_delta;"
              "pWriteProcessMemory(process, remote_ptr_address, (LPCVOID)&ptr, sizeof(ptr), NULL);"
            "}"
          "}"
          "remote_reloc_address = (PIMAGE_BASE_RELOCATION)((LPBYTE)remote_reloc_address + remote_reloc.SizeOfBlock);"
        "}"
        "reloc_delta = (ptrdiff_t)(dll_lua - " << luajit_nt_headers->OptionalHeader.ImageBase << ");"
        "remote_reloc_address = (PIMAGE_BASE_RELOCATION)(dll_lua + " << luajit_nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress << ");"
        "while (1) {"
          "IMAGE_BASE_RELOCATION remote_reloc;"
          "pReadProcessMemory(process, (LPCVOID)remote_reloc_address, (LPVOID)&remote_reloc, sizeof(remote_reloc), NULL);"
          "if (!remote_reloc.VirtualAddress) break;"
          "if (remote_reloc.SizeOfBlock > sizeof(remote_reloc)) {"
            "PWORD list = (PWORD)(remote_reloc_address + 1);"
            "for (size_t i = 0; i < (remote_reloc.SizeOfBlock - sizeof(remote_reloc)) / sizeof(WORD); i += 1) {"
              "pReadProcessMemory(process, (LPCVOID)(list + i), (LPVOID)&offset, sizeof(offset), NULL);"
              "if (!offset) continue;"
              "LPVOID remote_ptr_address = (LPVOID)(dll_lua + remote_reloc.VirtualAddress + (offset & 0xFFF));"
              "pReadProcessMemory(process, (LPCVOID)remote_ptr_address, (LPVOID)&ptr, sizeof(ptr), NULL);"
              "ptr += reloc_delta;"
              "pWriteProcessMemory(process, remote_ptr_address, (LPCVOID)&ptr, sizeof(ptr), NULL);"
            "}"
          "}"
          "remote_reloc_address = (PIMAGE_BASE_RELOCATION)((LPBYTE)remote_reloc_address + remote_reloc.SizeOfBlock);"
        "}" << std::endl;
    // set rwx permissions for each section
    std::cout << "DWORD oldp;" << std::endl;
    for (size_t i = 0; i < plugin_nt_headers->FileHeader.NumberOfSections; i += 1) {
        DWORD permission = perms_for_characteristics(plugin_section_header[i].Characteristics);
        std::cout << "pVirtualProtectEx(process, (LPVOID)(dll_plugin + " << plugin_section_header[i].VirtualAddress << "), " << plugin_section_header[i].Misc.VirtualSize << ", " << permission << ", &oldp);" << std::endl;
    }
    for (size_t i = 0; i < luajit_nt_headers->FileHeader.NumberOfSections; i += 1) {
        DWORD permission = perms_for_characteristics(luajit_section_header[i].Characteristics);
        std::cout << "pVirtualProtectEx(process, (LPVOID)(dll_lua + " << luajit_section_header[i].VirtualAddress << "), " << luajit_section_header[i].Misc.VirtualSize << ", " << permission << ", &oldp);" << std::endl;
    }
    // invoke entrypoint
    const IMAGE_DATA_DIRECTORY luajit_exception_dir = luajit_nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    std::cout << "struct PluginInjectParams params = {"
        ".kernel32=kernel32,"
        ".pGetModuleHandleW=pGetModuleHandleW,"
        ".pGetProcAddress=pGetProcAddress,"
        ".plugin=(HMODULE)dll_plugin,"
        ".luajit=(HMODULE)dll_lua,"
#if defined(_WIN64)
        ".luajit_exception_directory=(PIMAGE_RUNTIME_FUNCTION_ENTRY)(dll_lua + " << luajit_exception_dir.VirtualAddress << "),"
        ".luajit_rtl_entrycount=" << (luajit_exception_dir.Size / sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY)) << ","
#endif
        ".plugin_import_directory=(PIMAGE_IMPORT_DESCRIPTOR)(dll_plugin + " << plugin_nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress << "),"
        ".luajit_import_directory=(PIMAGE_IMPORT_DESCRIPTOR)(dll_lua + " << luajit_nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress << "),"
        ".luajit_export_directory=(PIMAGE_EXPORT_DIRECTORY)(dll_lua + " << luajit_nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress << ")"
    "};" << std::endl;
    std::cout << "struct PluginInjectParams* remote_params = (struct PluginInjectParams*)pVirtualAllocEx(process, NULL, sizeof(params), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);" << std::endl;
    std::cout << "pWriteProcessMemory(process, remote_params, &params, sizeof(params), NULL);" << std::endl;
    std::cout << "HANDLE remote_thread = pCreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)(dll_plugin + " << entry_point_rva << "), remote_params, 0, NULL);" << std::endl;
    std::cout << "pWaitForSingleObject(remote_thread, INFINITE);" << std::endl;
    std::cout << "pVirtualFreeEx(process, remote_params, 0, MEM_RELEASE);" << std::endl;

    // wipe dll headers from remote process
    std::cout << "pWriteProcessMemory(process, dll_plugin, (PVOID)zeroes, " << plugin_nt_headers->OptionalHeader.SizeOfHeaders << ", NULL);" << std::endl;
    std::cout << "pWriteProcessMemory(process, dll_lua, (PVOID)zeroes, " << luajit_nt_headers->OptionalHeader.SizeOfHeaders << ", NULL);" << std::endl;

    std::cout << "}" << std::endl;

    CloseHandle(luajit_file);
    CloseHandle(plugin_file);
    VirtualFree(luajit_dll, luajit_file_size, MEM_RELEASE);
    VirtualFree(plugin_dll, plugin_file_size, MEM_RELEASE);
    return 0;
}
