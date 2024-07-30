#include <Windows.h>
#include "dll_inject.h"

DWORD __stdcall BOLT_STUB_ENTRYNAME(struct PluginInjectParams* data) {
    return 0;
}
