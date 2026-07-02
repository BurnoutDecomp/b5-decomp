#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (base)
#include "GameSource/Gui/Flow/Shared/Components/BrnComplexBar.h"      // BrnGui::ComplexBar (embedded)

// BrnGui::PlayerStatsBar - the car-stats bar row (speed/boost/strength) on the
// player-stats panels: a GuiComponent wrapping one ComplexBar ("RedBar_mc").
// DWARF home BrnPlayerStatsBar.h:45. This TU bodies Construct +
// HandleTransitionComplete; Reset/SetCar are their own ledger functions.
namespace BrnGui
{
    struct PlayerStatsBar : public CgsGui::GuiComponent
    {
        // @0x824E5B10 (this TU, DWARF cpp:44) -- base Construct + construct the bar.
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // @0x824E5B60 (this TU, DWARF cpp:81) -- when the named transition is this
        // bar's, forward it; true when handled.
        bool HandleTransitionComplete(const char* lpcComponentName, s32 liArg);

        // DWARF cpp:63 / h:99 -- their own ledger functions (declaration-only here).
        void Reset(s32 liValue, u32 luMax);
        void SetCar(s32 liValue, u32 luMax);

    private:
        static const s32 KI_MAX_BAR_LENGTH = 10;   // DWARF h:76

        // DWARF cpp:21 -- the bar's component name ("RedBar_mc").
        static const char KAC_CAR_BAR_NAME[10];

        ComplexBar mCarBar;      // DWARF h:80 (X360 +0x8C)
        s32        miCarValue;   // DWARF h:82
    };
}
