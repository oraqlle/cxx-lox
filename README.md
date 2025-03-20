# clox

Implementation of Bob Nystrom's Lox language in C following his book
[Crafting Interpreters](https://craftinginterpreters.com/).

Lox is a simple dynamically-typed language similar in syntax to Python. It supports
imperitive style structures like `if`, `while` and `for` statements as well as functions.
It also has basic OOP capabilities such as classes and inheritence.

clox is a simple bytecode compiler and VM unified into a single simple interpreter
architecture meaning your Lox scripts are compiled to bytecode and executed on the fly.

This interpreter was used as a learning tool and is not recommended for professional
use.

## Building

If you wish to build clox yourself you will need CMake (>=v3.21) and any relatively
recent C compiler. This project has a relatively simple CMake config utilising CMake
presets to control flags and build options.

```sh
cmake -S . -B build --preset=<platform>
cmake --build build
./build/clox
```

Available platforms:

- linux
- linux-dev
- macos
- macos-dev
- win64
- win64-dev
- sanitize : Turns on santizers on Linux platforms
- linux-dev-strict : Additional checks made on linux using `clang-tidy` and `cpp-check`

> Note: There are addition targets that can be built using the `-t` flag during the build
> step called `spell-check`, `spell-fix`, `format-check` and `format-fix`. These require
> `clang-format` and `codespell` to work correctly.

## Changes

This project is almost entirely par-for-par with Bob Nystrom's version from his book.
It differs in setup with this project using a somewhat proper CMake config. I also do
not like the usage of global variables in Nystrom's version and thus mine weaves the
global variables throughout the compiler and VM as needed.

I also made [jlox](https://github.com/oraqlle/jlox) based on the first part of Nystrom's
book.

