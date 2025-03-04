add_compile_definitions(CLOX_DEVELOPER_MODE)

add_custom_target(
    run
    COMMAND clox
    VERBATIM
)

add_dependencies(run clox)

include(cmake/lint-targets.cmake)
include(cmake/spell-targets.cmake)
