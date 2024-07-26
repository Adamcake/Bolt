#include <Windows.h>
#include <stdio.h>

// this function is invoked via CreateRemoteThread by the manual map injector (see stub_inject)
// after it injects it into the process.
// since this function is called before main, it is not safe to call libc functions (e.g. fopen)
// because those would probably crash. windows.h functions are fine though (e.g. OpenFile).
DWORD __stdcall BOLT_STUB_ENTRYNAME(void* data) {
    return 0;
}
