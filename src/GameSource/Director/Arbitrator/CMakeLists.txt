target_sources(${PROJECT_NAME}
    PRIVATE
        BrnDirectorArbitrator.cpp
        BrnDirectorArbitratorSharedCameraContainer.cpp
        BrnDirectorArbitratorState.cpp
        BrnDirectorArbitratorStateContainer.cpp

    PUBLIC
        BrnDirectorArbitrator.h
        BrnDirectorArbitratorState.h
        BrnDirectorArbitratorStateContainer.h
        BrnDirectorArbitratorUtils.h
)

add_subdirectory(States)
