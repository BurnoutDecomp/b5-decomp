target_sources(${PROJECT_NAME}
    PRIVATE
        BrnTakedownManager.cpp
        BrnTakedownManager_Detect.cpp
        BrnTakedownManagerDebugComponent.cpp
        BrnTakedownManagerRaceCarData.cpp
        EventQueue_TakedownEvent_8.cpp

    PUBLIC
        BrnTakedownManager.h
        BrnTakedownManagerDebugComponent.h
        BrnTakedownManagerTypes.h
)
