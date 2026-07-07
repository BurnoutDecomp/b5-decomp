// ===================================================================================
// BrnGui::CrashNavPanel  -- implementation
//   class:BrnGui::CrashNavPanel
//
// Typed accessors for the crash-nav map panel's current selection. Each guards its read with
// a CGS_ASSERT that the panel is in the matching display sub-mode (+0x90):
//
//   GetPanelActiveGameModeType @ 0x824BAE58
//     Asserts the panel is showing events (+0x90 == 0), then returns the progression mode
//     for the highlighted event by mapping the embedded event panel's meCurrentGameMode
//     (+0x2EA8+0x8AC) through EventPanel::ConvertLocalEventDefToProgressionEventDef.
//   GetPanelActiveRoadRuleType @ 0x824185C8
//     Asserts the panel is showing road rules (+0x90 == 2), then returns the active
//     road-rule type (+0x481C).
//   GetRoadPanelScoreMode @ 0x82418668
//     Asserts the panel is showing road rules (+0x90 == 2), then returns the active
//     road-rule scoring mode (+0x4820).
//
// Reconstructed from the X360 asm; the assert branch tests (beq on cmpwi 0 / cmpwi 2) fix the
// required sub-mode, and the message literals are verbatim X360 rodata. The constructor
// (@0x82500FD0) is NOT reconstructed here -- it installs ~33 embedded sub-panel/toggle vtables
// from types that are not yet homed (see BrnCrashNavPanel.h).
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/Components/BrnCrashNavPanel.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGui
{
    // @ 0x824BAE58
    BrnProgression::RaceEventData::EModeType CrashNavPanel::GetPanelActiveGameModeType()
    {
        CGS_ASSERT(meShowingMode == E_SHOWING_MODE_EVENTS,
                   "Cannot get active game mode type if not showing events");   // @0x824BAE58 (beq on +0x90==0)

        return mEventPanel.ConvertLocalEventDefToProgressionEventDef(mEventPanel.meCurrentGameMode);
    }

    // @ 0x824185C8
    s32 CrashNavPanel::GetPanelActiveRoadRuleType() const
    {
        CGS_ASSERT(meShowingMode == E_SHOWING_MODE_ROAD_RULES,
                   "Cannot get active road rules type if not showing road rules");   // @0x824185C8 (beq on +0x90==2)

        return miActiveRoadRuleType;   // +0x481C
    }

    // @ 0x82418668
    s32 CrashNavPanel::GetRoadPanelScoreMode() const
    {
        CGS_ASSERT(meShowingMode == E_SHOWING_MODE_ROAD_RULES,
                   "Cannot get active road rules scoring mode if not showing road rules");   // @0x82418668 (beq on +0x90==2)

        return miActiveRoadRuleScoreMode;   // +0x4820
    }
}
