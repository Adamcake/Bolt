/*
When launching HDOS, we point its java.home setting to this program so that we can intercept its
attempt to run Java. This is necessary to fix a bug in HDOS-launcher which prevents it from working
on Java 17+, which the HDOS developers have outright refused to fix for some reason.
*/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    char** args = malloc(argc + 1);
    char* java_path = getenv("BOLT_JAVA_PATH");
    if (!java_path) return 1;
    if (argc) args[0] = java_path;
    for (size_t i = 1; i < argc; i += 1) {
        if (!strcmp(argv[i], "--add-opens java.desktop/java.awt=ALL-UNNAMED")) {
            argv[i][11] = '=';
        }
        args[i] = argv[i];
    }
    args[argc] = 0;
    unsetenv("BOLT_JAVA_PATH");
    execv(java_path, args);
    // exec will never return and libc will clean up our address space, so no need to call free()
}
