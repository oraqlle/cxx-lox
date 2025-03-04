install(
    TARGETS clox
    RUNTIME COMPONENT clox_runtime
)

if(PROJECT_IS_TOP_LEVEL)
    include(CPack)
endif()
