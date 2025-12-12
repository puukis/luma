cmake_minimum_required(VERSION 3.16)
project(luma LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(lumac
  src/main.cpp
  src/lexer.cpp
  src/parser.cpp
  src/ast_printer.cpp
  src/environment.cpp
  src/interpreter.cpp
)

target_include_directories(lumac PRIVATE src)
