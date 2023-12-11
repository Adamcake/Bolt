/*
When launching HDOS, we point its java.home setting to this program so that we can intercept its
attempt to run Java. This is necessary to fix a bug in HDOS-launcher which prevents it from working
on Java 17+, which the HDOS developers have outright refused to fix for some reason.

We also take this opportunity to set the JVM's user.home variable to something more appropriate.
*/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char** environ;

int main(int argc, char** argv) {
    char** args = malloc(argc + 2);
    char** args_ptr;
    char* java_path = getenv("BOLT_JAVA_PATH");
    char* bolt_home = getenv("BOLT_ARG_HOME");
    if (!java_path) return 1;
    if (bolt_home) {
        args[0] = java_path;
        args[1] = bolt_home;
        args_ptr = args;
    } else {
        args[1] = java_path;
        args_ptr = args + 1;
    }
    for (size_t i = 1; i < argc; i += 1) {
    if (!strcmp(argv[i], "--add-opens java.desktop/java.awt=ALL-UNNAMED")) {
        argv[i][11] = '=';
    }
        args[i + 1] = argv[i];
    }
    args[argc + 1] = 0;
    execve(java_path, args_ptr, environ);
}
