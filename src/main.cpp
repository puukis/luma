#include <fstream>
#include <iostream>
#include <sstream>

#include "ast_printer.hpp"
#include "interpreter.hpp"
#include "lexer.hpp"
#include "parser.hpp"

static std::string readFile(const std::string &path) {
  std::ifstream in(path, std::ios::in | std::ios::binary);
  if (!in)
    throw std::runtime_error("Could not open file: " + path);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

static void usage() {
  std::cerr << "Usage:\n"
            << "  lumac tokens <file.lu>\n"
            << "  lumac ast    <file.lu>\n"
            << "  lumac run    <file.lu>\n"
            << "  lumac <file.lu>        (defaults to run)\n";
}

int main(int argc, char **argv) {
  try {
    std::string mode = "run";
    std::string file;

    if (argc == 2) {
      file = argv[1];
    } else if (argc == 3) {
      mode = argv[1];
      file = argv[2];
    } else {
      usage();
      return 2;
    }

    std::string source = readFile(file);

    Lexer lexer(source);
    auto tokens = lexer.scanTokens();

    if (mode == "tokens") {
      for (const auto &t : tokens) {
        std::cout << t.line << "  " << tokenTypeName(t.type) << "  \""
                  << t.lexeme << "\"\n";
      }
      return 0;
    }

    Parser parser(tokens);
    auto program = parser.parse();

    if (mode == "ast") {
      AstPrinter printer;
      std::cout << printer.print(program);
      return 0;
    }

    if (mode == "run") {
      Interpreter interp;
      interp.run(program);
      return 0;
    }

    std::cerr << "Unknown mode: " << mode << "\n";
    usage();
    return 2;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
