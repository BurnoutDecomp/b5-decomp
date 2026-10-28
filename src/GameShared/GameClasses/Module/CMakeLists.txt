target_sources(${PROJECT_NAME}
    PRIVATE
        CgsModule.cpp
        CgsDataBuffer.cpp
        CgsDataStructure.cpp
        CgsModuleSingleBuffered.cpp

    PUBLIC
        CgsModule.h
        CgsDataBuffer.h
        CgsDataStructure.h
        CgsModuleSingleBuffered.h
        CgsModuleSingleBufferedTemplate.h
)
