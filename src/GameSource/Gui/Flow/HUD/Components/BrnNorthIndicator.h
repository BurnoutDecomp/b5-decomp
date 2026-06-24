#pragma once

// ===================================================================================
// BrnGui::NorthIndicatorComponent -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/HUD/Components/BrnNorthIndicator.h
//
// The HUD compass / "north indicator" GUI component. It drives two apt view-state outputs:
//   * "_rotation" -- the compass needle angle (radians -> degrees + 180), formatted "%f".
//   * "_style"    -- the per-game-mode visual style ("Race" / "RoadRage" / etc.).
//
// Derives from CgsGui::GuiComponent (the apt-view-state output plumbing). Layout/method set
// from the DecFIGS DWARF (BrnNorthIndicator.h:44/:47/:64/:68).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent base
#include "GameSource/GameState/BrnGameStateSharedIO.h"                // GameStateModuleIO::EGameModeType

namespace BrnGui
{
    struct NorthIndicatorComponent : public CgsGui::GuiComponent
    {
        // DWARF BrnNorthIndicator.h:47 -- the per-mode compass style. Indexes the "_style"
        // apt view-state string table (mpacStyleAptStrings).
        enum E_STYLES
        {
            E_STYLE_RACE          = 0,
            E_STYLE_ROADRAGE      = 1,
            E_STYLE_TRAFFICATTACK = 2,
            E_STYLE_PURSUIT       = 3,
            E_STYLE_ELIMINATOR    = 4,
            E_STYLE_BURNINGROUTE  = 5,
            E_STYLE_SHOWTIME      = 6,
            E_STYLE_COUNT         = 7,
        };

        // @ 0x824113B8 -- publish the compass needle rotation. lfHeadingRadians is converted to
        // degrees (* 57.295776) and offset by 180, formatted "%f" into the "_rotation" view state.
        void Update(f32 lfHeadingRadians);

        // @ 0x82411430 -- map the active game mode to a compass style and publish it as the
        // "_style" view state. An unknown mode asserts and falls back to E_STYLE_RACE.
        void SetEventType(BrnGameState::GameStateModuleIO::EGameModeType leGameMode);
    };
}
