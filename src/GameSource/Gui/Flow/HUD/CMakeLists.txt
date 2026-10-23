target_sources(${PROJECT_NAME}
    PRIVATE
        BrnHudFlow.cpp

    PUBLIC
        BrnHudFlow.h
)

add_subdirectory(Components)
add_subdirectory(States)
