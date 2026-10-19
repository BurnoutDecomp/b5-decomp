target_sources(${PROJECT_NAME}
    PRIVATE
        BurnoutConstants.cpp

    PUBLIC
        BurnoutConstants.h
)

add_subdirectory(Game)
add_subdirectory(Main)
