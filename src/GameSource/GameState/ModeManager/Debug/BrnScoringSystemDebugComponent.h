#pragma once

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                                            // EActiveRaceCarIndex
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"   // CgsDev::DebugComponent (real base)

// BrnGameState::ScoringSystemDebugComponent - the in-game "Scoring System" debug HUD overlay. Derives
// from the real CgsDev::DebugComponent: OnActivate registers a single "Show chainable stunts" bool
// tweakable with the debug menu, and DebugRenderChainableStunts draws the live chainable-stunt
// multiplier table (a column per player x a row per stunt-multiplier band) through the BUFFERED debug
// 2D renderer (a stack CgsDev::DebugInterface -> Get2dRender()). The owner (BrnGameState::ScoringSystem
// / the ModeManager) drives the render each frame from ModeManager::PreWorldUpdate. Recovered from the
// DecFIGS DWARF (GameState/ModeManager/Debug/BrnScoringSystemDebugComponent.h) and the X360 pseudocode/
// asm (GetName @0x82312470, OnActivate @0x82312490, GetChainableTableEntry @0x82329D60,
// DebugRenderChainableStunts @0x82337C38).
//
// Layout mirrors the sibling debug components (CrashScoreDebugComponent / ModeManagerDebugComponent):
// after the CgsDev::DebugComponent base, mpScoringSystem at +0x0C and mbShowChainableStunts at +0x10
// (the X360 asserts this+0xC "mpScoringSystem" and gates the render on *(this+0x10)).

// Forward declarations for the live state DebugRenderChainableStunts / GetChainableTableEntry reach;
// the full types are pulled in by the .cpp to keep this header's include surface small.
namespace BrnGameState { class ScoringSystem; }
namespace BrnNetwork { namespace BrnNetworkModuleIO { struct InGamePlayerStatusInterface; } }
namespace BrnWorld { namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; } }

namespace BrnGameState
{
    class ScoringSystemDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // @ 0x82337C38 - draw the live chainable-stunt multiplier table for the current mode. Asserts
        // mpScoringSystem is wired; only renders when mbShowChainableStunts is set AND the mode has at
        // least one car (ScoringSystem::muCarsInCurrentMode > 0). Acquires a stack DebugInterface,
        // draws a backing box + a (rows x columns) grid of labelled cells through the buffered 2D
        // renderer, then thread-safe-releases the manager.
        void DebugRenderChainableStunts(const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarOutput,
                                        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface* lpPlayerStatusInterface,
                                        s32 liMaxMultiplier,
                                        bool lbThirtyFps);

    protected:
        const char* GetName() const override;   // @ 0x82312470 ("Scoring System")
        void        OnActivate() override;       // @ 0x82312490

    private:
        // @ 0x82329D60 - fill one table cell. Produces the cell text in lpcOutBuffer (200-byte) and the
        // cell colour in *lpuOutColour for the (liColumn, liRow) cell. liColumn 0 = the row-header
        // column (a fixed label per row); liColumn>=1 = a player column (liColumn-1 indexes the network
        // player list). liRow 0 = the player-name header; liRow>=1 = a chainable-stunt-multiplier band.
        void GetChainableTableEntry(s32 liColumn, s32 liRow,
                                    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarOutput,
                                    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface* lpPlayerStatusInterface,
                                    s32 liMaxMultiplier,
                                    bool lbThirtyFps,
                                    char* lpcOutBuffer,
                                    u32* lpuOutColour);

        // Layout (after the CgsDev::DebugComponent base): mpScoringSystem at +0x0C, mbShowChainableStunts
        // at +0x10.
        ScoringSystem* mpScoringSystem;       // +0x0C
        bool           mbShowChainableStunts; // +0x10
    };
}
