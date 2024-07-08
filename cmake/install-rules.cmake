install(
    TARGETS clox
    RUNTIME COMPONENT clox_Runtime
)

if(PROJECT_IS_TOP_LEVEL)
    include(CPack)
endif()
