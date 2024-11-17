cmake_minimum_required(VERSION 3.21)

include(cmake/prelude.cmake)

project(
    clox
    VERSION 0.1.0
    DESCRIPTION "Lox Bytecode Interpreter in C"
    LANGUAGES C
)

include(cmake/variables.cmake)

# ---- Declare library ----
add_library(
    clox_lib OBJECT
    src/lib/chunk.c
    src/lib/object.c
    src/lib/memory.c
    src/lib/debug.c
    src/lib/value.c
    src/lib/vm.c
    src/lib/compiler.c
    src/lib/scanner.c
)

target_include_directories(
    clox_lib ${warning_guard}
    PUBLIC
    "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_compile_features(clox_lib PUBLIC c_std_99)

# ---- Declare executable ----
add_executable(clox src/bin/main.c)
add_executable(clox::exe ALIAS clox)

set_property(TARGET clox PROPERTY OUTPUT_NAME clox)

target_compile_features(clox PRIVATE c_std_99)
target_link_libraries(clox PRIVATE clox_lib)

# ---- Install rules ----
if(NOT CMAKE_SKIP_INSTALL_RULES)
    include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----
if(NOT CLOX_DEVELOPER_MODE)
    return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
    message(
        AUTHOR_WARNING
        "Developer mode is intended for developers of clox"
    )
endif()

include(cmake/dev-mode.cmake)

