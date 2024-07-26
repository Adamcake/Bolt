#include <Windows.h>
#include <stdio.h>

// this function is invoked via CreateRemoteThread by the manual map injector (see stub_inject)
// after it injects it into the process.
void BOLT_STUB_ENTRYNAME(void* data) {
    printf("stub hello world\n");
}
