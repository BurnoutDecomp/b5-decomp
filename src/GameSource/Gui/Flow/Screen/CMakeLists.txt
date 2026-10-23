target_sources(${PROJECT_NAME}
    PRIVATE
        BrnScreenFlow.cpp

    PUBLIC
        BrnScreenFlow.h
)

add_subdirectory(Components)
add_subdirectory(States)
