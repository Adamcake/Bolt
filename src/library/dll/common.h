// for functions used by more than one DLL-related target
#include <Windows.h>

#if defined(__cplusplus)
extern "C" {
#endif

/// take an nt section's "characteristics" and return the equivalent value for VirtualProtect
DWORD perms_for_characteristics(DWORD characteristics);

/// compare two strings.
/// true if match, false if not. for case-insensitive mode a2 must be all lowercase.
BOOL bolt_cmp(const char*, const char*, BOOL case_sensitive);

#if defined(__cplusplus)
}
#endif
