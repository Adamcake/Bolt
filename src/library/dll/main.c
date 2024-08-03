#include <Windows.h>

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "dll_inject.h"
#include "../plugin/plugin.h"
#include "../gl.h"

typedef HMODULE(WINAPI* LOADLIBRARYW)(LPCWSTR);
typedef HMODULE(WINAPI* GETMODULEHANDLEW)(LPCWSTR);
typedef FARPROC(WINAPI* GETPROCADDRESS)(HMODULE, LPCSTR);
typedef BOOL(WINAPI* VIRTUALPROTECT)(LPVOID, SIZE_T, DWORD, PDWORD);

typedef HGLRC(WINAPI* WGLCREATECONTEXT)(HDC);
typedef BOOL(WINAPI* WGLDELETECONTEXT)(HGLRC);
typedef PROC(WINAPI* WGLGETPROCADDRESS)(LPCSTR);
typedef BOOL(WINAPI* WGLMAKECURRENT)(HDC, HGLRC);

typedef HGLRC(__stdcall* WGLCREATECONTEXTATTRIBSARB)(HDC, HGLRC, const int*);

static WGLCREATECONTEXT real_wglCreateContext;
static WGLDELETECONTEXT real_wglDeleteContext;
static WGLGETPROCADDRESS real_wglGetProcAddress;
static WGLMAKECURRENT real_wglMakeCurrent;

static WGLCREATECONTEXTATTRIBSARB real_wglCreateContextAttribsARB;

static HGLRC WINAPI hook_wglCreateContext(HDC);
static BOOL WINAPI hook_wglDeleteContext(HGLRC);
static PROC WINAPI hook_wglGetProcAddress(LPCSTR);
static BOOL WINAPI hook_wglMakeCurrent(HDC, HGLRC);

static HGLRC __stdcall hook_wglCreateContextAttribsARB(HDC, HGLRC, const int*);

 CRITICAL_SECTION wgl_lock;

static struct GLLibFunctions libgl = {0};

#define VERBOSE

#if defined(VERBOSE)
static FILE* logfile = 0;
#define LOG(...) if(fprintf(logfile, __VA_ARGS__))fflush(logfile)
#else 
#define LOG(...)
#endif

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

    // get the functions we need from libgl
    HMODULE libgl_module = data->pGetModuleHandleW(L"opengl32.dll");
    libgl.BindTexture = (void(*)(GLenum, GLuint))data->pGetProcAddress(libgl_module, "glBindTexture");
    libgl.Clear = (void(*)(GLbitfield))data->pGetProcAddress(libgl_module, "glClear");
    libgl.ClearColor = (void(*)(GLfloat, GLfloat, GLfloat, GLfloat))data->pGetProcAddress(libgl_module, "glClearColor");
    libgl.DeleteTextures = (void(*)(GLsizei, const GLuint*))data->pGetProcAddress(libgl_module, "glDeleteTextures");
    libgl.DrawArrays = (void(*)(GLenum, GLint, GLsizei))data->pGetProcAddress(libgl_module, "glDrawArrays");
    libgl.DrawElements = (void(*)(GLenum, GLsizei, GLenum, const void*))data->pGetProcAddress(libgl_module, "glDrawElements");
    libgl.Flush = (void(*)(void))data->pGetProcAddress(libgl_module, "glFlush");
    libgl.GenTextures = (void(*)(GLsizei, GLuint*))data->pGetProcAddress(libgl_module, "glGenTextures");
    libgl.GetError = (GLenum(*)(void))data->pGetProcAddress(libgl_module, "glGetError");
    libgl.TexParameteri = (void(*)(GLenum, GLenum, GLfloat))data->pGetProcAddress(libgl_module, "glTexParameteri");
    libgl.TexSubImage2D = (void(*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*))data->pGetProcAddress(libgl_module, "glTexSubImage2D");
    libgl.Viewport = (void(*)(GLint, GLint, GLsizei, GLsizei))data->pGetProcAddress(libgl_module, "glViewport");
    real_wglCreateContext = (WGLCREATECONTEXT)data->pGetProcAddress(libgl_module, "wglCreateContext");
    real_wglDeleteContext = (WGLDELETECONTEXT)data->pGetProcAddress(libgl_module, "wglDeleteContext");
    real_wglGetProcAddress = (WGLGETPROCADDRESS)data->pGetProcAddress(libgl_module, "wglGetProcAddress");
    real_wglMakeCurrent = (WGLMAKECURRENT)data->pGetProcAddress(libgl_module, "wglMakeCurrent");

    // write our function hooks to the main module import table...
    DWORD oldp;
    HMODULE main_module = data->pGetModuleHandleW(NULL);
    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)main_module;
    PIMAGE_NT_HEADERS nt_headers = (PIMAGE_NT_HEADERS)((LPBYTE)main_module + dos_header->e_lfanew);
    IMAGE_DATA_DIRECTORY import_directory = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (PIMAGE_IMPORT_DESCRIPTOR import_descriptor = (PIMAGE_IMPORT_DESCRIPTOR)((LPBYTE)main_module + import_directory.VirtualAddress); import_descriptor->Name; import_descriptor += 1) {
        LPBYTE module_name = (LPBYTE)main_module + import_descriptor->Name;
        PIMAGE_THUNK_DATA lookup_table = (PIMAGE_THUNK_DATA)((LPBYTE)main_module + import_descriptor->OriginalFirstThunk);
        PIMAGE_THUNK_DATA address_table = (PIMAGE_THUNK_DATA)((LPBYTE)main_module + import_descriptor->FirstThunk);
        while (lookup_table->u1.AddressOfData) {
            if((lookup_table->u1.AddressOfData & IMAGE_ORDINAL_FLAG) == 0) {
                PIMAGE_IMPORT_BY_NAME fname = (PIMAGE_IMPORT_BY_NAME)((LPBYTE)main_module + lookup_table->u1.AddressOfData);
#define FNHOOK(NAME, FTYPE) if(bolt_cmp(fname->Name, #NAME, 1)) { pVirtualProtect((LPVOID)(&address_table->u1.Function), sizeof(address_table->u1.Function), PAGE_READWRITE, &oldp); address_table->u1.Function = (ULONGLONG)hook_##NAME; pVirtualProtect((LPVOID)(&address_table->u1.Function), sizeof(address_table->u1.Function), oldp, &oldp); }
                if (bolt_cmp(module_name, "opengl32.dll", 0)) {
                    FNHOOK(wglCreateContext, WGLCREATECONTEXT)
                    FNHOOK(wglDeleteContext, WGLDELETECONTEXT)
                    FNHOOK(wglGetProcAddress, WGLGETPROCADDRESS)
                    FNHOOK(wglMakeCurrent, WGLMAKECURRENT)
                }
#undef FNHOOK
            }
            lookup_table += 1;
            address_table += 1;
        }
    }

    // init stuff
    InitializeCriticalSection(&wgl_lock);
    _bolt_plugin_on_startup();

    return 0;
}

static void* bolt_GetProcAddress(const char* proc) {
    void* ret = (void*)real_wglGetProcAddress((LPCSTR)proc);
    LOG("bolt_GetProcAddress(\"%s\") -> %llu\n", proc, (ULONGLONG)ret);
    return ret;
}

static HGLRC WINAPI hook_wglCreateContext(HDC hdc) {
    EnterCriticalSection(&wgl_lock);
#if defined(VERBOSE)
    if (!logfile) _wfopen_s(&logfile, L"bolt-log.txt", L"wb");
#endif
    HGLRC ret = real_wglCreateContext(hdc);
    if (ret) _bolt_gl_onCreateContext(ret, NULL, &libgl, bolt_GetProcAddress, false);
    LeaveCriticalSection(&wgl_lock);
    LOG("wglCreateContext -> %llu\n", (ULONGLONG)ret);
    return ret;
}

static BOOL WINAPI hook_wglDeleteContext(HGLRC hglrc) {
    LOG("wglDeleteContext(%llu)\n", (ULONGLONG)hglrc);
    BOOL ret = real_wglDeleteContext(hglrc);
    if (ret) {
        EnterCriticalSection(&wgl_lock);
        _bolt_gl_onDestroyContext(hglrc);
        LeaveCriticalSection(&wgl_lock);
    }
    return ret;
}

static PROC WINAPI hook_wglGetProcAddress(LPCSTR proc) {
    LOG("wglGetProcAddress(\"%s\")\n", proc);
    if (!strcmp(proc, "wglCreateContextAttribsARB")) return (PROC)hook_wglCreateContextAttribsARB;
    void* ret = _bolt_gl_GetProcAddress(proc);
    return ret ? ret : real_wglGetProcAddress(proc);
}

static BOOL WINAPI hook_wglMakeCurrent(HDC hdc, HGLRC hglrc) {
    LOG("wglMakeCurrent(%llu)\n", (ULONGLONG)hglrc);

    BOOL ret = real_wglMakeCurrent(hdc, hglrc);
    if (ret) {
        EnterCriticalSection(&wgl_lock);
        real_wglCreateContextAttribsARB = (WGLCREATECONTEXTATTRIBSARB)real_wglGetProcAddress("wglCreateContextAttribsARB");
        _bolt_gl_onMakeCurrent(hglrc);
        LeaveCriticalSection(&wgl_lock);
    }
    LOG("wglMakeCurrent(%llu) -> %i\n", (ULONGLONG)hglrc, (int)ret);
    return ret;
}

static HGLRC __stdcall hook_wglCreateContextAttribsARB(HDC hdc, HGLRC hglrc, const int* attribList) {
    LOG("wglCreateContextAttribsARB(%llu)\n", (ULONGLONG)hglrc);
    EnterCriticalSection(&wgl_lock);
    HGLRC ret = real_wglCreateContextAttribsARB(hdc, hglrc, attribList);
    if (ret) _bolt_gl_onCreateContext(ret, hglrc, &libgl, bolt_GetProcAddress, true);
    LeaveCriticalSection(&wgl_lock);
    LOG("wglCreateContextAttribsARB(%llu) -> %llu\n", (ULONGLONG)hglrc, (ULONGLONG)ret);
    return ret;
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
