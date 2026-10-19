add_library(DirtySDK INTERFACE)

target_include_directories(DirtySDK INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_features(DirtySDK INTERFACE cxx_std_17)

add_library(EA::DirtySDK ALIAS DirtySDK)

add_subdirectory(src)
