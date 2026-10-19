add_library(PPMalloc INTERFACE)

target_sources(PPMalloc INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EAGeneralAllocator.cpp"
)

target_include_directories(PPMalloc INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_features(PPMalloc INTERFACE cxx_std_17)

add_library(EA::PPMalloc ALIAS PPMalloc)