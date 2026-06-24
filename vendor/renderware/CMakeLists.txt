# Renderware 4 core `rw::` type vocabulary (generated, header-only).
# See README.md. Regenerate with: python tools/renderware/generate_headers.py (repo root).
add_library(renderware STATIC IMPORTED)

set_target_properties(renderware PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/rwcore.lib"
)

set_target_properties(renderware PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
    INTERFACE_COMPILE_FEATURES "cxx_std_17"
)

add_library(rw::core ALIAS renderware)
