target_sources(${PROJECT_NAME}
    PRIVATE
        BrnGuiEffectsArbitrator.cpp
        BrnGuiPFXHooks.cpp
        BrnPfxHookBlender.cpp
        BrnPfxHookBlender_Fader.cpp
        CgsResourcePtr_BrnGui_PFXHookBundle.cpp

    PUBLIC
        BrnGuiEffectsArbitrator.h
        BrnGuiPFXHooks.h
        BrnPfxHookBlender.h
)

add_subdirectory(Resource)
