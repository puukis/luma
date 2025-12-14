#include "luma.h"

// Include the C++ headers for the Luma implementation
#include "interpreter.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

// This is a C++ helper function and should not have C linkage.
static std::string read_file_content(const std::string& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) {
        throw std::runtime_error("Could not open file: " + path);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// The extern "C" block ensures that the C++ compiler does not mangle the
// names of these functions, so they can be called from C code.
extern "C" {

// Cast the opaque C pointer back to the C++ Interpreter class pointer.
static Interpreter* as_cpp(LumaInterpreter* interp) {
    return reinterpret_cast<Interpreter*>(interp);
}

LumaInterpreter* luma_create() {
    // Create a C++ Interpreter object and cast it to the opaque C pointer.
    return reinterpret_cast<LumaInterpreter*>(new Interpreter());
}

void luma_destroy(LumaInterpreter* interp) {
    // Cast back and delete the C++ object.
    delete as_cpp(interp);
}

void luma_set_executable_path(LumaInterpreter* interp, const char* path) {
    as_cpp(interp)->setExecutablePath(path);
}

void luma_set_entry_file(LumaInterpreter* interp, const char* path) {
    as_cpp(interp)->setEntryFile(path);
}

int luma_run_string(LumaInterpreter* interp, const char* source, int is_repl) {
    try {
        Lexer lexer(source);
        auto tokens = lexer.scanTokens();
        Parser parser(tokens);
        auto program = parser.parse();
        as_cpp(interp)->run(program);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1; // Indicate failure
    }
    return 0; // Indicate success
}

int luma_run_file(LumaInterpreter* interp, const char* path) {
    try {
        luma_set_entry_file(interp, path);
        std::string source = read_file_content(path);
        // is_repl is 0 (false) because we are running a file.
        return luma_run_string(interp, source.c_str(), 0);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1; // Indicate failure
    }
    return 0; // Indicate success
}

} // extern "C"