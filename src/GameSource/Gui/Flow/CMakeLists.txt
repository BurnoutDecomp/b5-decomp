target_sources(${PROJECT_NAME}
    PRIVATE
        BrnBaseFlow.cpp
        BrnGuiEventFlowStates_embed_check.cpp

    PUBLIC
        BrnBaseFlow.h
)

add_subdirectory(HUD)
add_subdirectory(Overlay)
add_subdirectory(Permanent)
add_subdirectory(PostEvent)
add_subdirectory(PreEvent)
add_subdirectory(Screen)
add_subdirectory(Shared)
