project(Burnout5)

add_executable(${PROJECT_NAME} "")

target_compile_definitions(${PROJECT_NAME} PRIVATE
    D_PLATFORM_X360=1
    CGS_PLATFORM_X360=1
    _XBOX=1
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_sources(${PROJECT_NAME} PUBLIC types.hpp BrnCommonTypes.h)

target_link_libraries(${PROJECT_NAME} PUBLIC EABase EASTL EAThread rw::core EA::CoreAllocator EA::PPMalloc EA::DirtySDK zlib)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(GameSource)
add_subdirectory(GameShared)
