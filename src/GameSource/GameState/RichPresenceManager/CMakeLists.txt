target_sources(${PROJECT_NAME}
    PRIVATE
        BrnGameStateRichPresenceManagerBase.cpp

    PUBLIC
        BrnGameStateRichPresenceManagerBase.h
)

if (X360)
    add_subdirectory(X360)
endif()
