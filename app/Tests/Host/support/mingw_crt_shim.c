/* Link shim for TouchGFX's bundled MinGW (i686-w64-mingw32, msvcrt runtime).
 * Its libmingw32.a(ucrt_exit_wrappers.o) references the UCRT imports
 * quick_exit and _Exit, which msvcrt does not export, so any GoogleTest
 * binary fails to link. Both are routed to _exit. Host tests only. */
#include <stdlib.h>

static void ic_quick_exit(int code) { _exit(code); }
static void ic_Exit(int code) { _exit(code); }

void (*_imp__quick_exit)(int) = ic_quick_exit;
void (*_imp___Exit)(int) = ic_Exit;
