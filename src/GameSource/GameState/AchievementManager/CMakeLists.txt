target_sources(${PROJECT_NAME}
    PRIVATE
        BrnGameStateAchievementManagerBase.cpp
        BrnGameStateAchievementManagerBase_DriveThru.cpp
        BrnGameStateAchievementManagerBase_Freeburn.cpp
        BrnGameStateAchievementManagerBase_RoadRules.cpp

    PUBLIC
        BrnGameStateAchievementManagerBase.h
)

if (PS3)
    add_subdirectory(PS3)
endif()

if (X360)
    add_subdirectory(X360)
endif()
