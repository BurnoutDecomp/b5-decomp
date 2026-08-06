// ===================================================================================
// BrnGui::CrashNavPanel  -- implementation
//   class:BrnGui::CrashNavPanel
//
// Typed accessors for the crash-nav map panel's current selection. Each guards its read with
// a CGS_ASSERT that mePanelType (+0x90) is the matching sub-panel. The enum/member spellings
// are DWARF-supplied and corroborated by the X360 assert literals ("E_PANEL_EVENT ==
// mePanelType" etc.); an earlier revision of this file called them E_SHOWING_MODE_* /
// meShowingMode because it wrongly believed no DWARF existed for this class.
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
// required sub-mode, and the message literals are verbatim X360 rodata.
//
//   CrashNavPanel (default ctor) @ 0x82500FD0
//     Previously parked as blocked ("installs ~33 embedded sub-object vtables from un-homed
//     types"). Every collaborator is homed now (MenuToggleGroupVarSize<3> / EventPanel /
//     DriveThruMapPanel / RivalMapPanel / IconComponent / TextField), the header carries the
//     full DWARF member list, and the vtable rodata was dumped (single-slot Construct-only
//     tables, scratchpad vtables_cnp.json) -- so the ctor reduces to implicit member
//     default-construction with an empty body.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/Components/BrnCrashNavPanel.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGui
{
    // ---------------------------------------------------------------------------------
    // @ 0x82500FD0 -- the compiler-synthesised default ctor (the PS3 DWARF carries the
    // synthesised `CrashNavPanel()`; the one caller is CrashNavMap::CrashNavMap
    // @0x825114B8, constructing the by-value mCrashNavPanel at state+0x6E0). The X360
    // body is exactly:
    //   * its own single-slot vtable install (off_82074814 -> Construct @0x82425C60),
    //   * `bl` MenuToggleGroupVarSize<3>::MenuToggleGroupVarSize (@0x82500DB8) on
    //     mFilterToggles (this+0xA0) -- the only member ctor the compiler left
    //     out-of-line, and
    //   * 33 inlined sub-object vtable installs at the DWARF member offsets of the
    //     by-value panels / text fields / icon components (EventPanel @+0x2EA8 and its 6
    //     TextFields + 2 IconComponents; DriveThruMapPanel @+0x3788 + 2 TextFields;
    //     RoadPanel @+0x3A80 + RoadSignIcon + 9 TextFields + AnimationComponent;
    //     RivalMapPanel @+0x4830 + 4 TextFields + IconComponent; mGenericPanel @+0x4FD8;
    //     mGenericPanelText1/2 @+0x506C/+0x5194).
    // It stores NO data members (mePrepareStage/mePanelType/... are written later by
    // Construct / StoreSettings). All of the above is what C++ emits IMPLICITLY for this
    // member list, so the faithful reconstruction is an empty body -- the same reading
    // the committed MenuToggleGroupVarSize<N> and TextSelection ctors carry. The one
    // divergence is documented in the header: the mRoadPanel slot is still a byte-carve
    // (its region default-initialises to raw bytes) until BrnRoadPanel.h applies its
    // DWARF shape.
    CrashNavPanel::CrashNavPanel()
    {
    }

    // @ 0x824BAE58
    BrnProgression::RaceEventData::EModeType CrashNavPanel::GetPanelActiveGameModeType()
    {
        CGS_ASSERT(mePanelType == E_PANEL_EVENT,
                   "Cannot get active game mode type if not showing events");   // @0x824BAE58 (beq on +0x90==0)

        return mEventPanel.ConvertLocalEventDefToProgressionEventDef(mEventPanel.meCurrentGameMode);
    }

    // @ 0x824185C8
    s32 CrashNavPanel::GetPanelActiveRoadRuleType() const
    {
        CGS_ASSERT(mePanelType == E_PANEL_ROADSIGN,
                   "Cannot get active road rules type if not showing road rules");   // @0x824185C8 (beq on +0x90==2)

        return miActiveRoadRuleType;   // +0x481C == mRoadPanel.meCurrentRule (see header carve note)
    }

    // @ 0x82418668
    s32 CrashNavPanel::GetRoadPanelScoreMode() const
    {
        CGS_ASSERT(mePanelType == E_PANEL_ROADSIGN,
                   "Cannot get active road rules scoring mode if not showing road rules");   // @0x82418668 (beq on +0x90==2)

        return miActiveRoadRuleScoreMode;   // +0x4820 == mRoadPanel.meCurrentScoreMode (see header carve note)
    }
}
