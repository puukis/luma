#include "luma.h"
#include "linenoise.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Platform-specific headers for get_executable_path and home directory
#ifdef _WIN32
#include <windows.h>
#define PATH_MAX MAX_PATH
#else
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

// Forward declarations
static void run_repl(LumaInterpreter* interp);
static void get_executable_path(char* buffer, size_t size);
static void get_history_path(char* buffer, size_t size);


static void usage() {
    fprintf(stderr, "Usage: luma [options] [file.lu]\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -i         Run file and then enter interactive mode (REPL).\n");
    fprintf(stderr, "  --help     Show this help message.\n\n");
    fprintf(stderr, "If no file is provided, luma starts in REPL mode.\n");
}

int main(int argc, char** argv) {
#ifdef _WIN32
    // Enable UTF-8 output on Windows console for emojis and Unicode
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    // Enable ANSI escape sequences (for colors if needed)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif

    LumaInterpreter* interp = luma_create();
    if (!interp) {
        fprintf(stderr, "Fatal: Could not create Luma interpreter.\n");
        return 1;
    }

    char exe_path[PATH_MAX];
    get_executable_path(exe_path, sizeof(exe_path));
    luma_set_executable_path(interp, exe_path);

    int exit_code = 0;

    if (argc == 1) {
        run_repl(interp);
    } else if (argc == 2) {
        const char* arg = argv[1];
        if (strcmp(arg, "--help") == 0) {
            usage();
        } else {
            if (luma_run_file(interp, arg) != 0) {
                exit_code = 1; // File execution failed
            }
        }
    } else if (argc == 3) {
        const char* flag = argv[1];
        const char* file = argv[2];
        if (strcmp(flag, "-i") == 0) {
            if (luma_run_file(interp, file) == 0) {
                run_repl(interp);
            } else {
                exit_code = 1; // File execution failed
            }
        } else {
            usage();
            exit_code = 2;
        }
    } else {
        usage();
        exit_code = 2;
    }

    linenoiseHistoryFree();
    luma_destroy(interp);
    return exit_code;
}

static void run_repl(LumaInterpreter* interp) {
    char* line;
    char history_path[PATH_MAX];

    get_history_path(history_path, sizeof(history_path));

    printf("Luma REPL (v1.0, C Frontend)\n");
    printf("Type 'exit' or press Ctrl+C to quit.\n");
    
    // Load history and set max length
    linenoiseHistoryLoad(history_path);
    linenoiseHistorySetMaxLen(100);

    while((line = linenoise(">>> ")) != NULL) {
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            free(line);
            break;
        }

        if (line[0] != '\0') {
            luma_run_string(interp, line, 1);
            linenoiseHistoryAdd(line);
            linenoiseHistorySave(history_path);
        }

        free(line);
    }
}

static void get_history_path(char* buffer, size_t size) {
    const char* filename = ".luma_history";
#ifdef _WIN32
    const char* home_drive = getenv("HOMEDRIVE");
    const char* home_path = getenv("HOMEPATH");
    if (home_drive && home_path) {
        snprintf(buffer, size, "%s%s\\%s", home_drive, home_path, filename);
    } else {
        // Fallback if home env vars not set
        strncpy(buffer, filename, size);
    }
#else
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            home_dir = pw->pw_dir;
        }
    }
     if (home_dir) {
        snprintf(buffer, size, "%s/%s", home_dir, filename);
    } else {
        // Fallback
        strncpy(buffer, filename, size);
    }
#endif
}

static void get_executable_path(char* buffer, size_t size) {
#ifdef _WIN32
    // For Windows
    DWORD len = GetModuleFileNameA(NULL, buffer, size);
    if (len == 0) {
        buffer[0] = '\0'; // Failed
    }
#elif defined(__linux__)
    // For Linux
    ssize_t len = readlink("/proc/self/exe", buffer, size - 1);
    if (len != -1) {
        buffer[len] = '\0';
    } else {
        buffer[0] = '\0'; // Failed
    }
#else
    // Fallback for other platforms (e.g., macOS)
    // This is less reliable and may not work for all cases.
    // A more robust solution for macOS would use _NSGetExecutablePath.
    strncpy(buffer, "./luma", size); 
#endif
}
