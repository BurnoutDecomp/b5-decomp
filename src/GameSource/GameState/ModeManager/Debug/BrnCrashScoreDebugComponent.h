#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"      // CgsDev::DebugComponent (real base)
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"     // BrnGameState::CrashModeScoring (committed home)

// BrnGameState::CrashScoreDebugComponent - the in-game "Showtime score" debug HUD overlay for crash
// mode. Derives from the real CgsDev::DebugComponent: OnActivate registers two bool tweakables with
// the debug menu (the local "Display score" flag + the scoring object's "Infinite Crash Mode" flag),
// and RenderHUD draws the live crash-scoring readout (distance / cars leaped / combo / multiplier /
// best air time / highest jump) through the debug 2D immediate renderer when the flag is set.
// Recovered from the DecFIGS DWARF (GameState/ModeManager/Debug/BrnCrashScoreDebugComponent.h) and
// the X360 pseudocode/asm (Update @0x82312168, GetName @0x823121B0, OnActivate @0x823121C0,
// DisplayScores @0x82312210, RenderHUD @0x8231EB90). The component is embedded as the first member of
// BrnGameState::CrashModeScoring (BrnCrashModeScoring.h:215); it reaches that owner's live scoring
// state via mpCrashScoring (assigned by Construct, owned by the BrnCrashModeScoring.cpp TU).

namespace CgsDev { struct Debug2DImmediateRender; }

namespace BrnGameState
{
    class CrashScoreDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(CrashModeScoring* lpCrashScoring);   // @ BrnCrashScoreDebugComponent.cpp:47 (own TU)
        void Destruct();                                    // @ BrnCrashScoreDebugComponent.cpp:62 (own TU)

        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) override;   // @ 0x8231EB90
        void Update() override;                                             // @ 0x82312168

    protected:
        const char* GetName() const override;   // @ 0x823121B0
        const char* GetPath() const;             // @ BrnCrashScoreDebugComponent.cpp:124 (own TU)
        void        OnActivate() override;       // @ 0x823121C0

    private:
        void DisplayScores(CgsDev::Debug2DImmediateRender* lpRender);   // @ 0x82312210

        // Layout (after the CgsDev::DebugComponent base): mpCrashScoring at +0x0C, mbShowScoring at
        // +0x10 (RenderHUD asserts result[3]/+0xC, gates on *(this+0x10)). DWARF h:88 / h:90.
        CrashModeScoring* mpCrashScoring;   // +0x0C
        bool              mbShowScoring;    // +0x10
    };
}
