target_sources(${PROJECT_NAME}
    PRIVATE
        CgsStrStream.cpp

    PUBLIC
        CgsStrStream.h
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
