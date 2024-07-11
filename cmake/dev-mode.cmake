include(cmake/folders.cmake)

add_compile_definitions(CLOX_DEVELOPER_MODE)

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()

add_custom_target(
    run-exe
    COMMAND clox
    VERBATIM
)

add_dependencies(run-exe clox)

option(BUILD_MCSS_DOCS "Build documentation using Doxygen and m.css" OFF)

if(BUILD_MCSS_DOCS)
    include(cmake/docs.cmake)
endif()

option(ENABLE_COVERAGE "Enable coverage support separate from CTest's" OFF)

if(ENABLE_COVERAGE)
    include(cmake/coverage.cmake)
endif()

include(cmake/lint-targets.cmake)
include(cmake/spell-targets.cmake)

add_folders(Project)
