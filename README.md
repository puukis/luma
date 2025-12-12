# ☀️ Luma Programming Language

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)

Luma is a modern, interpreted programming language built from scratch in C++. It combines a clean, familiar syntax with unique, expressive features designed to make coding more intuitive and fun.

## 🚀 Key Features

Luma introduces several innovative paradigms alongside standard programming constructs:

- **Echo Loops**: A cleaner alternative to standard loops for repetition.
- **Swap Operator**: First-class syntax for swapping variables (`<->`).
- **Maybe Blocks**: A "try-without-fail" approach to error handling.
- **Dynamic Typing**: Supports Numbers, Strings, Booleans, and Functions.
- **First-Class Functions**: Closures and higher-order functions.

## 🛠️ Building & Installation

Luma is built with CMake and C++17.

### ⚡ Quick Install

#### macOS / Linux

To install Luma on macOS or Linux, run the following command:

```bash
curl -fsSL https://raw.githubusercontent.com/puukis/luma/refs/heads/main/scripts/install.sh | bash
```

#### Windows (PowerShell)

Run PowerShell as a normal user (no admin required) and execute:

```powershell
irm https://raw.githubusercontent.com/puukis/luma/refs/heads/main/scripts/install.ps1 | iex
```

> The Windows installer copies `lumac.exe` (and a `luma.exe` alias) to `%LOCALAPPDATA%\Programs\Luma` and adds that directory to your user `PATH` when possible.

### Prerequisites

- C++ Compiler (Clang/GCC/MSVC) supporting C++17
- CMake (3.12+)
- Git
- **Windows only:** Visual Studio Build Tools with C++ workload (or another compiler supported by CMake)

### Build Steps

```bash
mkdir build
cd build
cmake ..
make
```

On Windows (PowerShell or Developer Command Prompt):

```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

This produces the `lumac` executable.

## 💻 Usage

Run a Luma script:

```bash
./build/lumac run examples/test.lu
```

Print the AST (Abstract Syntax Tree):

```bash
./build/lumac ast examples/test.lu
```

Scan and print tokens:

```bash
./build/lumac tokens examples/test.lu
```

## ✨ Language Guide

### 1. Variables & Types

Variables are dynamic and declared by assignment.

```js
name = "Luma"; // String
ver = 1.0; // Number
is_fun = true; // Boolean
empty = nil; // Nil
```

### 2. Unique Features 🌟

#### Echo Loops

Repeat a block of code exactly N times without managing loop counters.

```js
echo 3 {
  print("Hello!")
}
// Prints "Hello!" 3 times
```

#### Swap Operator (`<->`)

Instantly swap the values of two variables.

```js
a = 10
b = 20
a <-> b
print(a) // 20
print(b) // 10
```

#### Maybe Blocks (`maybe` / `otherwise`)

Execute code that might fail. If it fails, execution continues seamlessly or jumps to the `otherwise` block.

```js
// Silently ignore errors
maybe {
  x = 1 / 0
}
print("I'm still alive!")

// Handle errors gracefully
maybe {
  call_undefined_function()
} otherwise {
  print("Something went wrong, but we caught it.")
}
```

#### Native Lists (`[...]`)

Create lists significantly easier.

```js
list = [1, 2, 3];
print(list[0]); // 1
print(list); // [1, 2, 3]
```

#### Until Loops (`until`)

Inverse of while loops. Runs until the condition is true.

```js
i = 0
until (i == 3) {
  print(i)
  i = i + 1
}
```

### 3. Control Flow

#### If / Else / Else If

Standard conditional logic.

```js
if (score > 90) {
  print("A");
} else if (score > 80) {
  print("B");
} else {
  print("Keep trying");
}
```

#### While Loops

```js
i = 0;
while (i < 5) {
  print(i);
  i = i + 1;
}
```

### 4. Functions

Functions are first-class citizens and support closures.

```js
def add(a, b) {
  return a + b
}

result = add(5, 10)

// Closures
def makeCounter() {
  i = 0
  def count() {
    i = i + 1
    print(i)
  }
  return count
}

counter = makeCounter()
counter() // 1
counter() // 2
```

## 🏗️ Project Structure

The project follows a modular interpreter architecture:

- **`main.cpp`**: CLI entry point.
- **`lexer.cpp` / `.hpp`**: Tokenizes source code.
- **`parser.cpp` / `.hpp`**: Parses tokens into an AST (Abstract Syntax Tree).
- **`ast.hpp`**: Defines AST nodes (Expr, Stmt).
- **`interpreter.cpp` / `.hpp`**: Walks the AST to execute the program.
- **`environment.cpp` / `.hpp`**: Manages variable scopes and memory.
- **`value.hpp`**: Defines the runtime value types (`std::variant`).

---

Built with ❤️ by Leonard Gunder.

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
