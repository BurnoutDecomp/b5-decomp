target_sources(${PROJECT_NAME}
    PRIVATE
        BrnCustomRenderer.cpp
        BrnGuiCustRendererDebugComponent.cpp

    PUBLIC
        BrnGuiCustRendererDebugComponent.h
)

add_subdirectory(Renderers)
