add_library(zlib STATIC)

target_sources(zlib
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include/zlib.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/zconf.h"

    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/adler32.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/crc32.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/infblock.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/infblock.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/infcodes.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/infcodes.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/inffast.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/inffast.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/inffixed.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/inflate.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/inftrees.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/inftrees.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/infutil.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/infutil.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/uncompr.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/zutil.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/zutil.h"
)

target_include_directories(zlib
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_features(zlib PRIVATE cxx_std_17)
