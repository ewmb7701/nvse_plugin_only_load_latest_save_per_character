#include "log.h"
#include <cstdarg>
#include <cstdio>

namespace { FILE* file = nullptr; }

void OpenLog(const char* gameDirectory) {
    if (file) { fclose(file); file = nullptr; }
    char path[1024];
    if (sprintf_s(path, "%sonly_load_latest_save_per_character.log", gameDirectory) > 0)
        fopen_s(&file, path, "w");
}

void Log(const char* format, ...) {
    if (!file) return;
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    fputc('\n', file);
    fflush(file);
}
