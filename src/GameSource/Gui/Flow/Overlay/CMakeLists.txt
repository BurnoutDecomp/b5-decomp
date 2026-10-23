target_sources(${PROJECT_NAME}
    PRIVATE
        BrnOverlayFlow.cpp

    PUBLIC
        BrnOverlayFlow.h
)

add_subdirectory(Components)
add_subdirectory(States)
