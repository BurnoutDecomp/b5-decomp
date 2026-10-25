project(Burnout5)

if (WIN32)
    add_executable(${PROJECT_NAME} WIN32 "")
else()
    add_executable(${PROJECT_NAME} "")
endif()

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_sources(${PROJECT_NAME} PUBLIC types.hpp BrnCommonTypes.h)

# Original GameTalk polymorphic-base provider, also named in the shipping list.
target_sources(${PROJECT_NAME} PRIVATE
    SDKs/Packages/GameTalk/1.8.0/source/IGameTalkProtocol.cpp
)

target_link_libraries(${PROJECT_NAME} EABase EASTL EAThread rw::core EA::CoreAllocator EA::PPMalloc EA::DirtySDK zlib)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(GameSource)
add_subdirectory(GameShared)
if (WIN32)
    add_subdirectory(pc)
    target_link_libraries(${PROJECT_NAME} Winmm.lib d3d9.lib ws2_32.lib)
endif()
