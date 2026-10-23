target_sources(${PROJECT_NAME}
    PRIVATE
        BrnCompletedGame.cpp
        BrnGuiOverlayPostEventStates_embed_check.cpp
        BrnOfflineInstantResults.cpp
        BrnOfflineRivalShutdown.cpp
        BrnOfflineTrophyCarUnlock.cpp

    PUBLIC
        BrnCompletedGame.h
        BrnOfflineInstantResults.h
        BrnOfflineRivalShutdown.h
        BrnOfflineTrophyCarUnlock.h
)
