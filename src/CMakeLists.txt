project(Burnout5)

if (WIN32)
    add_executable(${PROJECT_NAME} WIN32 "")
else()
    add_executable(${PROJECT_NAME} "")
endif()
target_sources(${PROJECT_NAME}
    PRIVATE
        GameSource/Game/BrnGameModule.cpp
        GameSource/Main/BrnMain.cpp
        GameShared/GameClasses/Module/CgsModule.cpp
        GameShared/GameClasses/Module/CgsDataBuffer.cpp
        GameShared/GameClasses/Module/CgsDataStructure.cpp
        GameShared/GameClasses/Module/CgsModuleSingleBuffered.cpp
        GameShared/GameClasses/Core/CgsStringUtils.cpp
        
    PUBLIC
        types.hpp
        GameShared/GameClasses/System/CgsHardwareInit.h
        GameSource/Game/BrnGameModule.hpp
        GameShared/GameClasses/Core/CgsAssert.h
        GameShared/GameClasses/Module/CgsModule.h
        GameShared/GameClasses/Module/CgsDataBuffer.h
        GameShared/GameClasses/Module/CgsDataStructure.h
        GameShared/GameClasses/Module/CgsModuleSingleBuffered.h
        GameShared/GameClasses/Module/CgsModuleSingleBufferedTemplate.h
        GameShared/GameClasses/Containers/CgsFlagSet.h
        GameShared/GameClasses/Core/CgsAssert.h
        GameShared/GameClasses/Core/CgsStringUtils.h
)

if (WIN32)
    target_sources(${PROJECT_NAME}
        PRIVATE
            GameShared/GameClasses/System/PC/CgsHardwareInitPC.cpp
            pc/gcm/renderengine/device.cpp
        PUBLIC
            pc/gcm/renderengine/device.h
    )
    target_link_libraries(${PROJECT_NAME} Winmm.lib d3d9.lib)
endif()
target_link_libraries(${PROJECT_NAME} EAThread)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/EABase/include/Common
        ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/EASTL/include
)
