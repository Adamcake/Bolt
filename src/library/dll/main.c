#include "../plugin/plugin.h"

#include <stdio.h>
#include <string.h>
#include <Windowsx.h>

#include "common.h"
#include "dll_inject.h"
#include "../gl.h"

typedef HMODULE(WINAPI* LOADLIBRARYW)(LPCWSTR);
typedef HMODULE(WINAPI* GETMODULEHANDLEW)(LPCWSTR);
typedef FARPROC(WINAPI* GETPROCADDRESS)(HMODULE, LPCSTR);
typedef BOOL(WINAPI* VIRTUALPROTECT)(LPVOID, SIZE_T, DWORD, PDWORD);
typedef HWND(WINAPI* CREATEWINDOWEXA)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
typedef LONG_PTR(WINAPI* SETWINDOWLONGPTRA)(HWND, int, LONG_PTR);
typedef LONG_PTR(WINAPI* GETWINDOWLONGPTRW)(HWND, int);

typedef HGLRC(WINAPI* WGLCREATECONTEXT)(HDC);
typedef BOOL(WINAPI* WGLDELETECONTEXT)(HGLRC);
typedef PROC(WINAPI* WGLGETPROCADDRESS)(LPCSTR);
typedef BOOL(WINAPI* WGLMAKECURRENT)(HDC, HGLRC);
typedef BOOL(WINAPI* SWAPBUFFERS)(HDC);

typedef HGLRC(__stdcall* WGLCREATECONTEXTATTRIBSARB)(HDC, HGLRC, const int*);

static WGLCREATECONTEXT real_wglCreateContext;
static WGLDELETECONTEXT real_wglDeleteContext;
static WGLGETPROCADDRESS real_wglGetProcAddress;
static WGLMAKECURRENT real_wglMakeCurrent;
static SWAPBUFFERS real_SwapBuffers;
static CREATEWINDOWEXA real_CreateWindowExA;
static SETWINDOWLONGPTRA real_SetWindowLongPtrA;
static GETWINDOWLONGPTRW real_GetWindowLongPtrW;

static WGLCREATECONTEXTATTRIBSARB real_wglCreateContextAttribsARB;

static HGLRC WINAPI hook_wglCreateContext(HDC);
static BOOL WINAPI hook_wglDeleteContext(HGLRC);
static PROC WINAPI hook_wglGetProcAddress(LPCSTR);
static BOOL WINAPI hook_wglMakeCurrent(HDC, HGLRC);
static BOOL WINAPI hook_SwapBuffers(HDC);
static HWND WINAPI hook_CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
static LONG_PTR WINAPI hook_SetWindowLongPtrA(HWND, int, LONG_PTR);
static LONG_PTR WINAPI hook_GetWindowLongPtrW(HWND, int);
static void hook_glGenTextures(GLsizei, GLuint*);
static void hook_glDrawElements(GLenum, GLsizei, GLenum, const void*);
static void hook_glDrawArrays(GLenum, GLint, GLsizei);
static void hook_glBindTexture(GLenum, GLuint);
static void hook_glTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
static void hook_glDeleteTextures(GLsizei, const GLuint*);
static void hook_glClear(GLbitfield);
static void hook_glViewport(GLint, GLint, GLsizei, GLsizei);
static void hook_glTexParameteri(GLenum, GLenum, GLint);

static HGLRC __stdcall hook_wglCreateContextAttribsARB(HDC, HGLRC, const int*);

CRITICAL_SECTION wgl_lock;

static struct GLLibFunctions libgl = {0};

static HWND game_parent_hwnd = 0;
static HWND game_hwnd = 0;
static WNDPROC game_wnd_proc = 0;
static int game_width = 0;
static int game_height = 0;

static HCURSOR cursor_default;

//#define VERBOSE

#if defined(VERBOSE)
static FILE* logfile = 0;
#define LOG(...) if(fprintf(logfile, __VA_ARGS__))fflush(logfile)
#else 
#define LOG(...)
#endif

static void resolve_imports(HMODULE, PIMAGE_IMPORT_DESCRIPTOR, HMODULE luajit, PIMAGE_EXPORT_DIRECTORY luajit_exports, GETMODULEHANDLEW, GETPROCADDRESS, LOADLIBRARYW, VIRTUALPROTECT);

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
    resolve_imports(data->luajit, data->luajit_import_directory, data->luajit, data->luajit_export_directory, data->pGetModuleHandleW, data->pGetProcAddress, pLoadLibraryW, pVirtualProtect);
    resolve_imports(data->plugin, data->plugin_import_directory, data->luajit, data->luajit_export_directory, data->pGetModuleHandleW, data->pGetProcAddress, pLoadLibraryW, pVirtualProtect);

    // get the functions we need from libgl
    HMODULE libgl_module = data->pGetModuleHandleW(L"opengl32.dll");
    libgl.BindTexture = (void(*)(GLenum, GLuint))data->pGetProcAddress(libgl_module, "glBindTexture");
    libgl.Clear = (void(*)(GLbitfield))data->pGetProcAddress(libgl_module, "glClear");
    libgl.ClearColor = (void(*)(GLfloat, GLfloat, GLfloat, GLfloat))data->pGetProcAddress(libgl_module, "glClearColor");
    libgl.DeleteTextures = (void(*)(GLsizei, const GLuint*))data->pGetProcAddress(libgl_module, "glDeleteTextures");
    libgl.Disable = (void(*)(GLenum))data->pGetProcAddress(libgl_module, "glDisable");
    libgl.DrawArrays = (void(*)(GLenum, GLint, GLsizei))data->pGetProcAddress(libgl_module, "glDrawArrays");
    libgl.DrawElements = (void(*)(GLenum, GLsizei, GLenum, const void*))data->pGetProcAddress(libgl_module, "glDrawElements");
    libgl.Enable = (void(*)(GLenum))data->pGetProcAddress(libgl_module, "glEnable");
    libgl.Flush = (void(*)(void))data->pGetProcAddress(libgl_module, "glFlush");
    libgl.GenTextures = (void(*)(GLsizei, GLuint*))data->pGetProcAddress(libgl_module, "glGenTextures");
    libgl.GetBooleanv = (void(*)(GLenum, GLboolean*))data->pGetProcAddress(libgl_module, "glGetBooleanv");
    libgl.GetError = (GLenum(*)(void))data->pGetProcAddress(libgl_module, "glGetError");
    libgl.GetIntegerv = (void(*)(GLenum, GLint*))data->pGetProcAddress(libgl_module, "glGetIntegerv");
    libgl.ReadPixels = (void(*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*))data->pGetProcAddress(libgl_module, "glReadPixels");
    libgl.TexParameteri = (void(*)(GLenum, GLenum, GLint))data->pGetProcAddress(libgl_module, "glTexParameteri");
    libgl.TexSubImage2D = (void(*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*))data->pGetProcAddress(libgl_module, "glTexSubImage2D");
    libgl.Viewport = (void(*)(GLint, GLint, GLsizei, GLsizei))data->pGetProcAddress(libgl_module, "glViewport");

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
#define FNHOOK(NAME) if(bolt_cmp(fname->Name, #NAME, 1)) { pVirtualProtect((LPVOID)(&address_table->u1.Function), sizeof(address_table->u1.Function), PAGE_READWRITE, &oldp); address_table->u1.Function = (ULONGLONG)hook_##NAME; pVirtualProtect((LPVOID)(&address_table->u1.Function), sizeof(address_table->u1.Function), oldp, &oldp); }
#define FNHOOKA(NAME, TYPE) if(bolt_cmp(fname->Name, #NAME, 1)) { pVirtualProtect((LPVOID)(&address_table->u1.Function), sizeof(address_table->u1.Function), PAGE_READWRITE, &oldp); real_##NAME = (TYPE)address_table->u1.Function; address_table->u1.Function = (ULONGLONG)hook_##NAME; pVirtualProtect((LPVOID)(&address_table->u1.Function), sizeof(address_table->u1.Function), oldp, &oldp); }
                if (bolt_cmp(module_name, "opengl32.dll", 0)) {
                    FNHOOKA(wglCreateContext, WGLCREATECONTEXT)
                    FNHOOKA(wglDeleteContext, WGLDELETECONTEXT)
                    FNHOOKA(wglGetProcAddress, WGLGETPROCADDRESS)
                    FNHOOKA(wglMakeCurrent, WGLMAKECURRENT)
                    FNHOOK(glGenTextures)
                    FNHOOK(glBindTexture)
                    FNHOOK(glDrawElements)
                    FNHOOK(glDrawArrays)
                    FNHOOK(glTexSubImage2D)
                    FNHOOK(glDeleteTextures)
                    FNHOOK(glTexParameteri)
                    FNHOOK(glClear)
                    FNHOOK(glViewport)
                }
                if (bolt_cmp(module_name, "gdi32.dll", 0)) {
                    FNHOOKA(SwapBuffers, SWAPBUFFERS)
                }
                if (bolt_cmp(module_name, "user32.dll", 0)) {
                    FNHOOKA(CreateWindowExA, CREATEWINDOWEXA)
                    FNHOOKA(GetWindowLongPtrW, GETWINDOWLONGPTRW)
                    FNHOOKA(SetWindowLongPtrA, SETWINDOWLONGPTRA)
                }
#undef FNHOOK
            }
            lookup_table += 1;
            address_table += 1;
        }
    }

#if defined(_WIN64)
    // call RtlAddFunctionTable for luajit dll because it implements error handling using SEH
    RtlAddFunctionTable(data->luajit_exception_directory, data->luajit_rtl_entrycount, (DWORD64)data->luajit);
#endif

    // init stuff
    InitializeCriticalSection(&wgl_lock);
    _bolt_plugin_on_startup();
    cursor_default = LoadImageW(NULL, (LPCWSTR)IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED | LR_DEFAULTSIZE);

    return 0;
}

static void winapi_to_mouseevent(int x, int y, WPARAM param, struct MouseEvent* out) {
    out->x = x;
    out->y = y;
    out->ctrl = param & MK_CONTROL ? 1 : 0;
    out->shift = param & MK_SHIFT ? 1 : 0;
    out->meta = (GetKeyState(VK_LWIN) & (1 << 15)) || (GetKeyState(VK_RWIN) & (1 << 15));
    out->alt = GetKeyState(VK_MENU) & (1 << 15) ? 1 : 0;
    out->capslock = GetKeyState(VK_CAPITAL) & 1;
    out->numlock = GetKeyState(VK_NUMLOCK) & 1;
    out->mb_left = param & MK_LBUTTON ? 1 : 0;
    out->mb_right = param & MK_RBUTTON ? 1 : 0;
    out->mb_middle = param & MK_MBUTTON ? 1 : 0;
}

static BOOL handle_mouse_event(WPARAM wParam, POINTS point, ptrdiff_t bool_offset, ptrdiff_t event_offset, uint8_t grab_type) {
    struct MouseEvent event;
    winapi_to_mouseevent(point.x, point.y, wParam, &event);
    const uint8_t ret = _bolt_plugin_handle_mouse_event(&event, bool_offset, event_offset, grab_type, NULL, NULL);
    if (!ret) SetCursor(cursor_default);
    return ret;
}

// middle-man function for WNDPROC. when the game window gets an event, we intercept it here, act on
// any information that's relevant to us, then invoke the actual game's WNDPROC if we want to.
// an example of a case when we wouldn't want to pass it to the game would be a mouse-click event
// which got captured by an embedded plugin window.
LRESULT hook_wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    LOG("WNDPROC(%u, %llu, %llu)\n", uMsg, (ULONGLONG)wParam, (ULONGLONG)lParam);
    switch (uMsg) {
        case WM_WINDOWPOSCHANGING:
        case WM_WINDOWPOSCHANGED: {
            const LPWINDOWPOS pos = (LPWINDOWPOS)lParam;
            if (!(pos->flags & SWP_NOSIZE)) {
                game_width = pos->cx;
                game_height = pos->cy;
            }
            break;
        }
        case WM_MOUSEMOVE:
            if (!handle_mouse_event(wParam, MAKEPOINTS(lParam), offsetof(struct WindowPendingInput, mouse_motion), offsetof(struct WindowPendingInput, mouse_motion_event), GRAB_TYPE_NONE)) return 0;
            break;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            if (!handle_mouse_event(wParam, MAKEPOINTS(lParam), offsetof(struct WindowPendingInput, mouse_left), offsetof(struct WindowPendingInput, mouse_left_event), GRAB_TYPE_START)) return 0;
            break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
            if (!handle_mouse_event(wParam, MAKEPOINTS(lParam), offsetof(struct WindowPendingInput, mouse_right), offsetof(struct WindowPendingInput, mouse_right_event), GRAB_TYPE_NONE)) return 0;
            break;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
            if (!handle_mouse_event(wParam, MAKEPOINTS(lParam), offsetof(struct WindowPendingInput, mouse_middle), offsetof(struct WindowPendingInput, mouse_middle_event), GRAB_TYPE_NONE)) return 0;
            break;
        case WM_LBUTTONUP:
            if (!handle_mouse_event(wParam, MAKEPOINTS(lParam), offsetof(struct WindowPendingInput, mouse_left_up), offsetof(struct WindowPendingInput, mouse_left_up_event), GRAB_TYPE_STOP)) return 0;
            break;
        case WM_RBUTTONUP:
            if (!handle_mouse_event(wParam, MAKEPOINTS(lParam), offsetof(struct WindowPendingInput, mouse_right_up), offsetof(struct WindowPendingInput, mouse_right_up_event), GRAB_TYPE_NONE)) return 0;
            break;
        case WM_MBUTTONUP:
            if (!handle_mouse_event(wParam, MAKEPOINTS(lParam), offsetof(struct WindowPendingInput, mouse_middle_up), offsetof(struct WindowPendingInput, mouse_middle_up_event), GRAB_TYPE_NONE)) return 0;
            break;
        case WM_MOUSEWHEEL: {
            const WORD keys = GET_KEYSTATE_WPARAM(wParam);
            const SHORT delta = (SHORT)GET_WHEEL_DELTA_WPARAM(wParam);
            POINTS points = MAKEPOINTS(lParam);
            POINT point = {.x = points.x, .y = points.y};
            ScreenToClient(hWnd, &point);
            if (point.x > 0xFFFF || point.y > 0xFFFF) break;
            points.x = (SHORT)point.x;
            points.y = (SHORT)point.y;
            if (delta > 0) {
                if (!handle_mouse_event(keys, points, offsetof(struct WindowPendingInput, mouse_scroll_up), offsetof(struct WindowPendingInput, mouse_scroll_up_event), GRAB_TYPE_NONE)) return 0;
            } else {
                if (!handle_mouse_event(keys, points, offsetof(struct WindowPendingInput, mouse_scroll_down), offsetof(struct WindowPendingInput, mouse_scroll_down_event), GRAB_TYPE_NONE)) return 0;
            }
            break;
        }
    }
    return game_wnd_proc(hWnd, uMsg, wParam, lParam);
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

static BOOL WINAPI hook_SwapBuffers(HDC hdc) {
    LOG("SwapBuffers\n");
    if (game_width > 0 && game_height > 0) {
        _bolt_gl_onSwapBuffers((uint32_t)game_width, (uint32_t)game_height);
    }
    BOOL ret = real_SwapBuffers(hdc);
    LOG("SwapBuffers -> %i\n", (int)ret);
    return ret;
}

static HWND WINAPI hook_CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
#if defined(VERBOSE)
    if (!logfile) _wfopen_s(&logfile, L"bolt-log.txt", L"wb");
#endif
    LOG("CreateWindowExA(class=\"%s\", name=\"%s\", x=%i, y=%i, w=%i, h=%i)\n", lpClassName, lpWindowName, X, Y, nWidth, nHeight);
    HWND ret = real_CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    if (!game_parent_hwnd) {
        game_parent_hwnd = ret;
    } else if (!game_hwnd && (hWndParent == game_parent_hwnd)) {
        game_hwnd = ret;
        game_wnd_proc = (WNDPROC)real_SetWindowLongPtrA(ret, GWLP_WNDPROC, (LONG_PTR)hook_wndproc);
        game_width = nWidth;
        game_height = nHeight;
    }
    return ret;
}

static LONG_PTR WINAPI hook_SetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong) {
    if (nIndex != GWLP_WNDPROC || (game_parent_hwnd != hWnd)) return real_SetWindowLongPtrA(hWnd, nIndex, dwNewLong);
    LONG_PTR ret;
    const WNDPROC old_game_wndproc = game_wnd_proc;
    if (dwNewLong == 0) {
        game_wnd_proc = 0;
        ret = real_SetWindowLongPtrA(hWnd, nIndex, dwNewLong);
    } else {
        game_wnd_proc = (WNDPROC)dwNewLong;
        ret = real_SetWindowLongPtrA(hWnd, nIndex, (LONG_PTR)hook_wndproc);
    }
    if (ret == (LONG_PTR)hook_wndproc) return (LONG_PTR)old_game_wndproc;
    return ret;
}

static LONG_PTR WINAPI hook_GetWindowLongPtrW(HWND hWnd, int nIndex) {
    const LONG_PTR ret = real_GetWindowLongPtrW(hWnd, nIndex);
    if (game_parent_hwnd == hWnd && ret == (LONG_PTR)hook_wndproc) return (LONG_PTR)game_wnd_proc;
    return ret;
}

static void hook_glGenTextures(GLsizei n, GLuint* textures) {
    LOG("glGenTextures(%i...)\n", n);
    libgl.GenTextures(n, textures);
    _bolt_gl_onGenTextures(n, textures);
    LOG("glGenTextures end\n", n);
}

static void hook_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
    LOG("glDrawElements\n");
    libgl.DrawElements(mode, count, type, indices);
    _bolt_gl_onDrawElements(mode, count, type, indices);
    LOG("glDrawElements end\n");
}

static void hook_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    LOG("glDrawArrays\n");
    libgl.DrawArrays(mode, first, count);
    _bolt_gl_onDrawArrays(mode, first, count);
    LOG("glDrawArrays end\n");
}

static void hook_glBindTexture(GLenum target, GLuint texture) {
    LOG("glBindTexture(%u, %u)\n", target, texture);
    libgl.BindTexture(target, texture);
    _bolt_gl_onBindTexture(target, texture);
    LOG("glBindTexture end\n");
}

static void hook_glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
    LOG("glTexSubImage2D\n");
    libgl.TexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    _bolt_gl_onTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    LOG("glTexSubImage2D end\n");
}

static void hook_glDeleteTextures(GLsizei n, const GLuint* textures) {
    LOG("glDeleteTextures\n");
    libgl.DeleteTextures(n, textures);
    _bolt_gl_onDeleteTextures(n, textures);
    LOG("glDeleteTextures end\n");
}

static void hook_glTexParameteri(GLenum target, GLenum pname, GLint param) {
    LOG("glTexParameteri\n");
    libgl.TexParameteri(target, pname, param);
    _bolt_gl_onTexParameteri(target, pname, param);
    LOG("glTexParameteri end\n");
}

static void hook_glClear(GLbitfield mask) {
    LOG("glClear\n");
    libgl.Clear(mask);
    _bolt_gl_onClear(mask);
    LOG("glClear end\n");
}

static void hook_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    LOG("glViewport\n");
    libgl.Viewport(x, y, width, height);
    _bolt_gl_onViewport(x, y, width, height);
    LOG("glViewport end\n");
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

static void resolve_imports(HMODULE image_base, PIMAGE_IMPORT_DESCRIPTOR import_, HMODULE luajit, PIMAGE_EXPORT_DIRECTORY luajit_exports, GETMODULEHANDLEW pGetModuleHandleW, GETPROCADDRESS pGetProcAddress, LOADLIBRARYW pLoadLibraryW, VIRTUALPROTECT pVirtualProtect) {
    // 4096 appears to be the maximum allowed name of a DLL or exported function, though they don't expose a constant for it
    WCHAR wname_buffer[4096];
    const PDWORD luajit_export_address_list = (PDWORD)((LPBYTE)luajit + luajit_exports->AddressOfFunctions);
    const PWORD luajit_export_name_ordinals = (PWORD)((LPBYTE)luajit + luajit_exports->AddressOfNameOrdinals);
    const PDWORD luajit_export_name_list = (PDWORD)((LPBYTE)luajit + luajit_exports->AddressOfNames);

    // loop once per dll in the import table
    for (PIMAGE_IMPORT_DESCRIPTOR import = import_; import->Characteristics; import += 1) {
        PIMAGE_THUNK_DATA orig_first_thunk = (PIMAGE_THUNK_DATA)((LPBYTE)image_base + import->OriginalFirstThunk);
        PIMAGE_THUNK_DATA first_thunk = (PIMAGE_THUNK_DATA)((LPBYTE)image_base + import->FirstThunk);
        LPCSTR dll_name = (LPCSTR)((LPBYTE)image_base + import->Name);

        if (bolt_cmp(dll_name, "lua51.dll", 0)) {
            // importing functions exported by manually-mapped lua module, so can't use GetProcAddress.
            // loop once per function imported from this particular dll in the import table
            while (orig_first_thunk->u1.AddressOfData) {
                ULONGLONG function = 0;
                if (orig_first_thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
                    // import by ordinal
                    function = (ULONGLONG)luajit + luajit_export_address_list[(orig_first_thunk->u1.Ordinal & 0xFFFF) - luajit_exports->Base];
                } else {
                    // import by name
                    PIMAGE_IMPORT_BY_NAME pibn = (PIMAGE_IMPORT_BY_NAME)((LPBYTE)image_base + orig_first_thunk->u1.AddressOfData);
                    for (size_t i = 0; i < luajit_exports->NumberOfNames; i += 1) {
                        const char* export_name = (const char*)((LPBYTE)luajit + luajit_export_name_list[i]);
                        if (!bolt_cmp(export_name, pibn->Name, 1)) continue;
                        function = (ULONGLONG)luajit + luajit_export_address_list[luajit_export_name_ordinals[i]];
                        break;
                    }
                }
                DWORD oldp;
                pVirtualProtect((LPVOID)(&first_thunk->u1.Function), sizeof(first_thunk->u1.Function), PAGE_READWRITE, &oldp);
                first_thunk->u1.Function = function;
                pVirtualProtect((LPVOID)(&first_thunk->u1.Function), sizeof(first_thunk->u1.Function), oldp, &oldp);
                orig_first_thunk += 1;
                first_thunk += 1;
            }
            continue;
        }

        size_t i = 0;
        while (1) {
            // this is not a great way of converting LPCSTR to LPCWSTR, but it's safe in this case
            // because DLL names are ASCII. generally you should use MultiByteToWideChar().
            if (!(wname_buffer[i] = (WCHAR)dll_name[i])) break;
            i += 1;
        }
        HMODULE hmodule = pGetModuleHandleW(wname_buffer);
        if (!hmodule) hmodule = pLoadLibraryW(wname_buffer);

        // loop once per function imported from this particular dll in the import table
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
    }
}
