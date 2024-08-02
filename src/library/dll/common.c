#include "common.h"

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

DWORD perms_for_characteristics(DWORD characteristics) {
    const BOOL readp = (characteristics & IMAGE_SCN_MEM_READ);
    const BOOL writep = (characteristics & IMAGE_SCN_MEM_WRITE);
    const BOOL execp = (characteristics & IMAGE_SCN_MEM_EXECUTE);
    return
        readp ?
            writep ?
                execp ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE
            :
                execp ? PAGE_EXECUTE_READ : PAGE_READONLY
        :
            writep ?
                execp ? PAGE_EXECUTE_WRITECOPY : PAGE_WRITECOPY
            :
                execp ? PAGE_EXECUTE : PAGE_NOACCESS;
}
