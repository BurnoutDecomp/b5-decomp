target_sources(${PROJECT_NAME}
    PRIVATE
        BrnTriggerQueryManager.cpp
        BrnTriggerQueryManager_Prepare.cpp
        BrnTriggerQueryManagerDebugComponent.cpp
        BrnTriggerQueryManagerDebugComponent_RenderWorld.cpp

    PUBLIC
        BrnKillzoneAction.h
        BrnTriggerQueryManager.h
        BrnTriggerQueryManagerDebugComponent.h
)
