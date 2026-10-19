add_library(CoreAllocator INTERFACE)

target_sources(CoreAllocator INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/source/icoreallocator_interface.cpp"
)

target_include_directories(CoreAllocator INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_features(CoreAllocator INTERFACE cxx_std_17)

add_library(EA::CoreAllocator ALIAS CoreAllocator)