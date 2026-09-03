#pragma once

#include "types.hpp"
#include "DebugSystem/Core/CgsDebugComponent.h"                              // CgsDev::DebugComponent (real base)
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"   // EActiveRaceCarIndex

// BrnGameState::TakedownManagerDebugComponent - the in-game debug menu for the takedown manager.
// Derives from the real CgsDev::DebugComponent; registers three display toggles + a "force
// takedown" action with the debug UI. Recovered from the DecFIGS DWARF (GameState/TakedownManager/
// BrnTakedownManagerDebugComponent.h). Incremental: the activation slice this TU implements
// (OnActivate / GetName / the ForceTakedown action) is reconstructed; the render + record methods
// are reconstructed by their own pass.

namespace BrnGameState
{
    struct TakedownManager;   // pointer member; full definition in BrnTakedownManager.h (the .cpp includes it)

    class TakedownManagerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // DWARF BrnTakedownManagerDebugComponent.cpp:42. No out-of-line X360 symbol: it is inlined
        // into GameStateModule::Construct @0x82380404..0x82380434 (the store cluster right after the
        // owning TakedownManager's two manager pointers). Body in the .cpp. [takedown wave 2026-09-02]
        void Construct(TakedownManager* lpTakedownManager);

        // DWARF BrnTakedownManagerDebugComponent.cpp:216 / X360 0x823663F8. Called by
        // TakedownManager::DetectStandardTakedown once a standard takedown has been classified:
        // remembers the pair and the victim's time-since-last-race-car-contact for the HUD.
        // Body in the .cpp. [takedown wave 2026-09-02, agent T2]
        void RecordTakedown(EActiveRaceCarIndex leAggressorIndex, EActiveRaceCarIndex leVictimIndex);

    protected:
        const char* GetName() const override;   // @ 0x823596C8
        void        OnActivate() override;       // @ 0x82366378

    private:
        // The "Force takedown" action callback registered with the debug menu; the void* context
        // the menu passes back is this component (registered via RegisterFunction(..., this, ...)).
        static void ForceTakedownCallback(void* lpContext);   // @ 0x823597F8
    public:
        // [takedown wave 2026-09-02] PC harness entry to the console's own "Force takedown" debug action
        // (the menu callback above), so a scripted run can exercise the takedown chain without a
        // 50-mph ram. Not an X360 symbol.
        void HarnessForceTakedown() { ForceTakedownCallback(this); }
    private:

        TakedownManager*    mpTakedownManager;
        EActiveRaceCarIndex meLastAggressorIndex;
        EActiveRaceCarIndex meLastVictimIndex;
        f32                 mfLastTakedownContactTime;
        bool                mbShowVulnerability;
        bool                mbShowLastTakedownInfo;
        bool                mbShowRevengeTakedownInfo;
    };
}
