#include <Windows.h>
#include "dll_inject.h"

// this function is invoked via CreateRemoteThread by the stub dll (see dll_inject).
// the plugin dll is injected into the suspended game process during a CreateProcessW hook, then it
// calls this function, awaits its return, and finally resumes the process.
// the import table is not resolved at this point. we rely on kernel32.dll having a stable address
// so we can assume GetModuleHandle and GetProcAddress will be the same as in any other process, so
// we can not only resolve our own IAT but also place hooks in the game's IAT.
// note that it is not safe to use any libc function from this function. doing so will probably crash.
DWORD __stdcall BOLT_STUB_ENTRYNAME(struct PluginInjectParams* data) {
    return 0;
}
