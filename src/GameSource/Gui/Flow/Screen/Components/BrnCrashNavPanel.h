#pragma once

// ===================================================================================
// BrnGui::CrashNavPanel  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnCrashNavPanel.h
//
// The "crash nav" map panel. It owns one sub-panel per selectable map entity (event /
// drive-thru / road sign / rival) plus a generic two-line text panel, multiplexes between
// them through ChangeVisiblePanelState, and exposes typed accessors for the currently
// active selection. Each accessor asserts the panel is showing the matching sub-panel
// before returning.
//
// DECFIGS DWARF *DOES* EXIST FOR THIS CLASS (a previous revision of this header claimed
// it did not, and invented names as a result):
//   references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Screen/Components/BrnCrashNavPanel.h
// It supplies the full member list, the `PanelType` / `AnimState` / `EPrepareStage`
// enums, and every declaration shape below. The X360 ARTIST asm corroborates the enum
// name and the enumerator spellings directly through the assert literals
// ("E_PANEL_EVENT == mePanelType" @0x8243A8B4, "(E_PANEL_DRIVETHRU == mePanelType) || ..."
// @0x8243A934, "E_PANEL_ROADSIGN == mePanelType" @0x8243A9B0, "E_PANEL_RIVALS ==
// mePanelType" @0x8243ABA4), each guarding a `lwz r11, 0x90(this)`.
//
// MEASURED X360 sub-object offsets (each is an `addi r3, this, N` immediately before a
// call into that sub-panel; console offsets, COMMENTS ONLY -- the host layout is
// name-based and every embedded pointer widens):
//   +0x0090  mePanelType          (lwz, the assert subject above)
//   +0x0098  mpGuiCache           (lwz; asserted non-NULL in SetupComponent @0x82440388)
//   +0x00A0  mFilterToggles       (MenuToggleGroupVarSize<3>, @0x82425F14 / Update's vcall)
//   +0x2EA8  mEventPanel          (@0x82425F24, @0x8243A8E0)
//   +0x3788  mDrivethruPanel      (@0x82425F34, @0x8243A954)
//   +0x3A80  mRoadPanel           (@0x82425F44, @0x8243A9E4, @0x824188E0)
//   +0x4830  mRivalPanel          (@0x82425F54, @0x8243AAFC, @0x8243ABCC)
//   +0x506C  mGenericPanelText1   (@0x8243A844, TextField::OutputAptData)
//   +0x5194  mGenericPanelText2   (@0x8243A854, TextField::OutputAptData)
//   +0x52BC  meSavedPanelType / +0x52C0 meSavedEventMode / +0x52C4 meSavedRRScoreType
//            (the StoreSettings triple @0x8241872C..0x82418734)
//
// PARTIAL LAYOUT: only the members the reconstructed accessors touch are carved; the rest
// of the object (the five sub-panels, the toggle group, the saved-settings triple) is
// still reserved byte-span so each named field lands at its binary offset without
// raw-offset casting. All access is by name.
//
// NOT MODELLED (deliberate, see the report for wave J):
//   * The DWARF base `struct CrashNavPanel : public CgsGui::GuiComponent` is NOT applied.
//     The head is still a reserved span, so applying the base would double-count it. That
//     is a layout redesign, not a declaration add, and it is what still blocks
//     CrashNavPanel::Construct (@0x82425C60, virtual slot 0) for CrashNavMap::OnEnter.
//   * `PanelType GetPanelActiveFilterMode()` (DWARF h:314) is NOT declared: it has no
//     out-of-line X360 body, so no TU owns it and nothing attests what it reads
//     (mePanelType @+0x90 or the toggle group @+0xA0 are both plausible). Adding it would
//     be an invented body.
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                        // typedef u64 CgsID
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                    // BrnGui::GuiFlow (E_GUIFLOW_*)
#include "GameSource/Gui/Flow/Screen/Components/BrnEventPanel.h"   // embedded EventPanel + EModeType

// Pointer/reference-only collaborators -- forward declared rather than included.
//   CgsModule::Event   : the shared opaque event base (defined in
//                        GameShared/GameClasses/Module/CgsVariableEventQueue.h); the same
//                        forward declaration CgsBaseEventReceiverQueue.h uses.
//   CgsNetwork::PlayerName : real home GameSource/GameState/BrnCgsPlayerName.h (already
//                        pulled in transitively by BrnGuiEventTypeDefs.h; declared again
//                        here so this header does not depend on that transitive path).
namespace CgsModule  { struct Event; }
namespace CgsNetwork { struct PlayerName; }

namespace BrnGui
{
    // Real home GameSource/Gui/BrnGuiCache.h -- pointer-only here.
    class GuiCache;

    // DWARF `struct BrnGui::RoadPanelData` (namespace scope, NOT nested in RoadPanel):
    // references/DecFIGS/dwarfdump/.../BrnRoadPanel.h:57. It has no committed home yet --
    // BrnRoadPanel.h is its owner-to-be -- and SetRoadPanelData only takes it by
    // reference, so an incomplete type suffices.
    struct RoadPanelData;

    class CrashNavPanel
    {
    public:
        // Which sub-panel the crash-nav panel is currently showing (object +0x90).
        // Names AND values from the DWARF (BrnCrashNavPanel.h:69); E_PANEL_EVENT /
        // E_PANEL_DRIVETHRU / E_PANEL_ROADSIGN / E_PANEL_RIVALS are independently attested
        // by the X360 assert literals listed in the banner, and their values 0/1/2/3 are
        // measured from ChangeVisiblePanelState's first argument at each setter
        // (@0x8243A8C4 li r4,0 / @0x8243A944 li r4,1 / @0x8243AA24 li r4,2 /
        //  @0x8243AAE0 li r4,3).
        enum PanelType
        {
            E_PANEL_EVENT            = 0,
            E_PANEL_DRIVETHRU        = 1,
            E_PANEL_ROADSIGN         = 2,
            E_PANEL_RIVALS           = 3,
            E_PANEL_SELECTABLE_COUNT = 4,   // the four above are the selectable filters
            E_PANEL_GENERIC          = 4,
            E_PANEL_COUNT            = 5,
        };

        // The 20-byte "challenged event scores" record CrashNavMap builds on its stack and
        // hands to SetEventPanelData, which forwards it to EventPanel::SetEventData.
        // MEASURED at the only site that passes a non-NULL pointer, CrashNavMap::Update
        // @0x824DDA58..0x824DDA78: word 0 is `lwz r11, 8(payload)` stored to the record
        // base, then `memcpy(record + 4, payload + 0xC, 0x10)`.
        // FLAG consumer-named: the X360 has no symbol for this record and no DWARF row --
        // SetEventPanelData is 2-argument on PS3 (see the note on the method below). The
        // trailing 16 bytes are copied but no reader for them was found, so they stay an
        // explicitly-reserved block rather than being guessed at.
        struct ChallengedEventScore
        {
            u32 muScoreOverride;      // +0x00  <- response payload +0x08
            u8  maReserved_04[16];    // +0x04  <- response payload +0x0C..+0x1B (memcpy 16)
        };

        // --------------------------------------------------------------------------
        // Bodies live in this class's own TU (BrnCrashNavPanel.cpp); declarations only.
        // Shapes are DWARF (BrnCrashNavPanel.h line numbers noted) gated on X360
        // attestation -- every one below has an X360 address in progress/identity.json.
        // --------------------------------------------------------------------------

        // @ 0x82418708 (DWARF h:249 / cpp:127). MEASURED BEHAVIOUR, which the name does not
        // convey: with the flag TRUE the panel does NOT store anything -- it writes the
        // K_DEFAULT_* triple (0, 5, 0) straight into meSavedPanelType/meSavedEventMode/
        // meSavedRRScoreType and returns (@0x82418724..0x82418734). Only with the flag
        // FALSE does it capture the live settings, and then only once mePrepareStage
        // (+0x8C) == E_PREPARESTAGE_DONE. CrashNavMap::Construct @0x824B6660 passes true.
        // FLAG inferred parameter name (the DWARF carries no parameter names).
        void StoreSettings(bool lbResetToDefaults);

        // @ 0x82425EC8 (DWARF h:195 / cpp:248). Asserts lpGuiCache non-NULL, then forwards
        // (leFlow, lpGuiCache) to the toggle group and all four sub-panels.
        void AppendExpectedAptComponents(GuiFlow leFlow, GuiCache* lpGuiCache);

        // @ 0x82440378 (DWARF h:198 / cpp:268).
        void SetupComponent();

        // @ 0x82418810 (DWARF h:201 / cpp:370). Six instructions: a tail call through slot
        // +0x14 of the vtable of the toggle group at +0xA0. void per the DWARF -- the `int`
        // Hex-Rays shows is the tail call, not a computed value.
        void Update();

        // @ 0x82441F58 (DWARF h:210 / cpp:520). THREE arguments. The body only reads r3/r4/r5
        // (@0x82441F64..0x82441F70), which is why Hex-Rays renders it 2-argument, but the
        // call site loads the third: `lwz r6, var_DC(r1)` @0x824DDA9C feeds it the event
        // size out-parameter from GetFirstEvent/GetNextEvent immediately before the `bl`
        // (0x824DDA9C..0x824DDAA4). The DWARF's third int32_t is therefore X360-real and
        // simply unused in this build's body.
        bool RecEvent(const CgsModule::Event* lpEvent, s32 liEventId, s32 liEventSize);

        // @ 0x8243A820 (DWARF h:213 / cpp:614). Switches to E_PANEL_GENERIC and blanks both
        // generic text fields.
        void ShowBlank();

        // @ 0x8243A878 (DWARF h:216 / cpp:634). THREE arguments on X360; the PS3 DWARF
        // declares (uint32_t, bool). The X360 inserts the score-record pointer in the
        // middle: the body forwards it as EventPanel::SetEventData's second data argument
        // (`mr r5, r29` @0x8243A8D4) alongside mpGuiCache (`lwz r6, 0x98`) and the flag
        // (`mr r7, r28`). The call site in CrashNavMap::Update passes three arguments --
        // `li r6,1` / `addi r5, r1, var_B0` / `mr r4, r30` @0x824DDA70..0x824DDA7C, with the
        // record built in the two instructions above it.
        // FLAG: the middle parameter is the X360-only delta over the DWARF shape.
        void SetEventPanelData(u32 luEventId, const ChallengedEventScore* lpScores,
                               bool lbShowPanel);

        // @ 0x8243A8F0 (DWARF h:219 / cpp:654). Asserts mePanelType is E_PANEL_DRIVETHRU or
        // E_PANEL_EVENT, switches to E_PANEL_DRIVETHRU, forwards to
        // DriveThruMapPanel::SetDriveThruData.
        void SetDrivethruPanelData(CgsID lDriveThruId);

        // @ 0x8243A978 (DWARF h:222 / cpp:675). Asserts "E_PANEL_ROADSIGN == mePanelType"
        // and "NULL != lpRoadName", then memcpy's 0x144 bytes of lrData into the embedded
        // road panel (@0x8243AA08..0x8243AA14). The reference is non-const per the DWARF.
        void SetRoadPanelData(const char* lpacRoadName, RoadPanelData& lrData);

        // @ 0x8243AAC8 (DWARF h:228 / cpp:721). The no-argument overload: picks the local
        // player's own rival row out of mpGuiCache.
        void SetRivalPanelData();

        // @ 0x8243AB68 (DWARF h:231 / cpp:757; the X360 export leaves this one unnamed as
        // sub_8243AB68 -- identified by its assert literal "E_PANEL_RIVALS == mePanelType"
        // at BrnCrashNavPanel.cpp:760, which is the DWARF's own line for this overload).
        // The id test is a 64-bit `cmpldi` @0x8243ABB0, consistent with CgsID == u64.
        void SetRivalPanelData(CgsID lRivalId);

        // @ 0x8243ABF0 (DWARF h:234 / cpp:784; unnamed sub_8243ABF0 in the X360 export,
        // identified by its assert at BrnCrashNavPanel.cpp:787). Forwards (lpName, lRivalId,
        // mpGuiCache) @0x8243AC44..0x8243AC54 to the same rival-panel setter the no-argument
        // overload reaches @0x8243AB4C -- which gets there by CONSTRUCTING a local
        // CgsNetwork::PlayerName (@0x8243AB38) and loading the id as a doubleword
        // (`ld r5, 0x4AF0(mpGuiCache)`). That pairing is what types both parameters.
        void SetRivalPanelData(const CgsNetwork::PlayerName* lpName, CgsID lRivalId);

        // @ 0x824188B0 (DWARF h:252 / cpp:1152). True when the road-rule scoreboard has a
        // real friend row selected: GetRoadPanelScoreMode() == 1 and the embedded road
        // panel's GetSelectedFriendName() differs from "-" (inlined strcmp
        // @0x824188EC..0x82418918). `const` is DWARF-attested.
        bool IsRoadRuleFriendSelected() const;

        // @ 0x824BAE58 - active game/progression mode for the highlighted event. Asserts the
        // panel is showing events, then maps the embedded event panel's current game mode
        // through EventPanel::ConvertLocalEventDefToProgressionEventDef. Non-const because that
        // collaborator is a non-const method.
        BrnProgression::RaceEventData::EModeType GetPanelActiveGameModeType();

        // @ 0x824185C8 - active road-rule type. Asserts the panel is showing road rules.
        // DWARF DELTA: declared `BrnStreetData::ScoreType GetPanelActiveRoadRuleType() const`
        // (h:346). Kept as s32 here because the committed consumers compare the result
        // against plain integers; retyping it is a separate, consumer-visible change.
        s32 GetPanelActiveRoadRuleType() const;

        // @ 0x82418668 - active road-rule scoring mode. Asserts the panel is showing road rules.
        // DWARF DELTA: declared
        // `BrnGui::GuiEventSetRoadRuleScoreMode::ERoadPanelModes GetRoadPanelScoreMode() const`
        // (h:363). Same reasoning as above -- left as s32.
        s32 GetRoadPanelScoreMode() const;

    private:
        // Reserved storage for the unrecovered panel head that precedes mePanelType
        // (object +0x90). Per the DWARF this span is the CgsGui::GuiComponent base plus
        // mePrepareStage (+0x8C). Layout-recovery padding only.
        u8           maHeadReserved[0x90];

        PanelType    mePanelType;     // +0x90  (lwz; the assert subject)

        // Reserved span between mePanelType and the embedded event panel (object +0x2EA8).
        // Per the DWARF this covers mpGuiCache (+0x98), meVisiblePanel and mFilterToggles
        // (+0xA0).
        u8           maReservedToEventPanel[0x2EA8 - (0x90 + sizeof(PanelType))];

        EventPanel   mEventPanel;     // +0x2EA8  (event-info sub-panel)

        // Reserved span between the event panel and the road-rule fields (object +0x481C).
        // Per the DWARF this covers mDrivethruPanel (+0x3788) and mRoadPanel (+0x3A80).
        u8           maReservedToRoadRule[0x481C - (0x2EA8 + sizeof(EventPanel))];

        s32          miActiveRoadRuleType;        // +0x481C  (lwz)
        s32          miActiveRoadRuleScoreMode;   // +0x4820  (lwz)
    };
}
