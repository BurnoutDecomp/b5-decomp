target_sources(${PROJECT_NAME}
    PRIVATE
        BurnoutConstants.cpp

    PUBLIC
        BurnoutConstants.h
)

add_subdirectory(AttribSys)
add_subdirectory(CompilerDefines)
add_subdirectory(Director)
add_subdirectory(Effects)
add_subdirectory(Game)
add_subdirectory(GameFlowController)
add_subdirectory(Main)
