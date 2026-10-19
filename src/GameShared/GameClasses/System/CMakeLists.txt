target_sources(${PROJECT_NAME}
    PRIVATE

    PUBLIC
        CgsHardwareInit.h
)

if (WIN32)
    add_subdirectory(PC)
endif()
