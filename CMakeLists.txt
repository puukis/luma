cmake_minimum_required(VERSION 3.16)
project(luma LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(luma
  src/main.c
  src/luma_api.cpp
  src/lexer.cpp
  src/parser.cpp
  src/ast_printer.cpp
  src/environment.cpp
  src/interpreter.cpp
  src/third_party/linenoise/linenoise.cpp
  src/third_party/linenoise/ConvertUTF.cpp
  src/third_party/linenoise/wcwidth.cpp
)

target_include_directories(luma PRIVATE src src/third_party/linenoise)
