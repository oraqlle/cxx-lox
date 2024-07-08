cmake_minimum_required(VERSION 3.21)

include(cmake/prelude.cmake)

project(
    clox
    VERSION 0.1.0
    DESCRIPTION "Lox Bytecode Interpreter in C"
    HOMEPAGE_URL "https://example.com/"
    LANGUAGES C
)

include(cmake/variables.cmake)

# ---- Declare executable ----
add_executable(clox src/bin/main.c)
add_executable(clox::exe ALIAS clox)

set_property(TARGET clox PROPERTY OUTPUT_NAME clox)

target_compile_features(clox PRIVATE c_std_99)

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

