#pragma once

// This header provides a C-compatible interface for the Luma interpreter.

#ifdef __cplusplus
extern "C" {
#endif

// Opaque pointer to the C++ Interpreter object to hide implementation details.
typedef struct LumaInterpreter LumaInterpreter;

// Creates a new Luma interpreter instance.
// Must be freed with luma_destroy().
LumaInterpreter* luma_create();

// Destroys an interpreter instance and frees associated memory.
void luma_destroy(LumaInterpreter* interp);

// Sets the path of the executable. Used to locate the standard library.
void luma_set_executable_path(LumaInterpreter* interp, const char* path);

// Sets the path of the entry script. Used to resolve module paths.
void luma_set_entry_file(LumaInterpreter* interp, const char* path);

// Executes a string of Luma source code.
// Returns 0 on success, 1 on failure.
// In REPL mode, errors are printed to stderr but do not terminate.
int luma_run_string(LumaInterpreter* interp, const char* source, int is_repl);

// Reads and executes a Luma source file.
// Returns 0 on success, 1 on failure.
int luma_run_file(LumaInterpreter* interp, const char* path);

#ifdef __cplusplus
} // extern "C"
#endif
