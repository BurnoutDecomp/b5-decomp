# RenderWare 4 `rw::` type vocabulary. Runtime bodies are
# reconstructed from ARTIST under src/ and mounted by the owning build target.
add_library(renderware INTERFACE)

target_include_directories(renderware INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_features(renderware INTERFACE cxx_std_17)

add_library(rw::core ALIAS renderware)
