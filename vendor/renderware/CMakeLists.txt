# Renderware 4 core `rw::` type vocabulary (generated, header-only).
# See README.md. Regenerate with: python tools/gen_rwcore_headers.py (repo root).
add_library(renderware INTERFACE)
target_include_directories(renderware INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(renderware INTERFACE cxx_std_17)
add_library(rw::core ALIAS renderware)
