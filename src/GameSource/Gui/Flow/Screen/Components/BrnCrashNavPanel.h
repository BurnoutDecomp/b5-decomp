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
// FULL DWARF LAYOUT (2026-08-04). The member list below is the complete DWARF sequence
// (BrnCrashNavPanel.h:237..:264) -- mePrepareStage .. meSavedRRScoreType covers the whole
// object (console sizeof == 0x52C8: mGenericPanelText2 0x5194 + 0x128 == 0x52BC == the
// StoreSettings triple). Every member is typed at its committed home EXCEPT mRoadPanel
// (DWARF h:248), which stays a documented reserved carve -- see the note at that slot.
// The default ctor @0x82500FD0 is thereby reconstructable as plain member default-
// construction; its 33 X360 stores were verified against the vtable rodata by headless
// IDA (scratchpad vtables_cnp.json): each installed vtable is a SINGLE-slot (virtual
// Construct only) table -- off_82074814[0]==CrashNavPanel::Construct @0x82425C60,
// off_8207317C/80/84/88[0]==EventPanel/DriveThruMapPanel/RoadPanel/RivalMapPanel::
// Construct, off_82072F8C[0]==TextField::Construct, and off_82072F68/off_82072F90/
// off_820564F0[0]==the inherited CgsGui::GuiComponent::Construct (AnimationComponent /
// IconComponent / RoadSignIcon do not override it). All access is by name.
//
// BASE APPLIED (2026-08-03). The DWARF base `struct BrnGui::CrashNavPanel : public
// CgsGui::GuiComponent` is now spelled out, and the old `u8 maHeadReserved[0x90]` that
// stood in for it is replaced by the ONE member the DWARF puts between the base and
// mePanelType: `EPrepareStage mePrepareStage` (h:237, X360 +0x8C -- exactly where the
// 32-bit GuiComponent layout ends: vptr 0x00 / macName[128] 0x04 / muHashedName 0x84 /
// mpStateInterface 0x88). Nothing is double-counted: the reserved span WAS the base plus
// that member, and both are now named. This is what makes the DWARF's virtual
// `Construct(const char*, StateInterface*, const char*)` (@0x82425C60, vtable slot 0)
// declarable, which CrashNavMap::OnEnter calls through slot 0 (`lwz r10, 0x6E0(r31);
// lwz r11, 0(r10); bctrl` @0x824CB2D0..0x824CB2F0). As everywhere else in this header the
// X360 offsets are COMMENTS ONLY -- the host base widens and all access is by name.
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                        // typedef u64 CgsID
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                    // BrnGui::GuiFlow (E_GUIFLOW_*); pulls BrnStreetData::ScoreType
#include "GameSource/Gui/Flow/Screen/Components/BrnEventPanel.h"   // embedded EventPanel + EEventType
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // CgsGui::GuiComponent (DWARF base)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.h"    // MenuToggleGroupVarSize<3> (mFilterToggles, by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnDriveThruMapPanel.h"  // DriveThruMapPanel (mDrivethruPanel, by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnRivalMapPanel.h"      // RivalMapPanel (mRivalPanel, by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"               // IconComponent (mGenericPanel, by value)
#include "GameSource/Gui/BrnGuiTextField.h"                              // TextField (mGenericPanelText1/2, by value)

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

    class CrashNavPanel : public CgsGui::GuiComponent
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

        // DWARF BrnCrashNavPanel.h:103. Names and values verbatim; StoreSettings gates its
        // capture branch on mePrepareStage == E_PREPARESTAGE_DONE.
        enum EPrepareStage
        {
            E_PREPARESTAGE_CONSTRUCTED = 0,
            E_PREPARESTAGE_DONE        = 1,
            E_PREPARESTAGE_COUNT       = 2,
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

        // @ 0x82500FD0 -- the compiler-synthesised default ctor (the PS3 DWARF carries the
        // synthesised `CrashNavPanel()` declaration; called by CrashNavMap::CrashNavMap
        // @0x825114B8 for the by-value mCrashNavPanel). Its X360 body is: its own vtable
        // install (off_82074814, whose single slot is Construct @0x82425C60), ONE
        // out-of-line member-ctor call (`bl` MenuToggleGroupVarSize<3>::ctor @0x82500DB8
        // on mFilterToggles, this+0xA0), and 33 inlined sub-object vtable installs whose
        // addresses are exactly the DWARF member offsets of the by-value panels/fields
        // below. It stores NO data members (Construct/StoreSettings do that later), so the
        // reconstruction is an empty body: C++ emits the base/member construction
        // implicitly -- the same reading MenuToggleGroupVarSize's and TextSelection's
        // committed ctors already carry. Body in BrnCrashNavPanel.cpp.
        CrashNavPanel();

        // @ 0x82425C60 (DWARF cpp:85), the CgsGui::GuiComponent virtual at vtable slot 0.
        // CrashNavMap::OnEnter reaches it through the vtable rather than by name:
        // `addi r3, r31, 0x6E0; addi r4, "CrashNavPanel_mc"; lwz r5, 0x1C(r31); li r6, 0;
        //  lwz r11, 0(r10); mtctr r11; bctrl` @0x824CB2D4..0x824CB2F0 -- three arguments,
        // the third (the parent name) NULL. Body links from the CrashNavPanel TU.
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

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

        // DWARF h:314 -- which sub-panel the panel is currently showing. It has no
        // out-of-line X360 body (the DWARF gives it a .h line, i.e. it is defined inline in
        // the original header), so NO TU will ever home it and a bare declaration would be
        // an unresolved external the compile-only gate cannot see. Written inline over the
        // named member instead, and the member it reads is now MEASURED rather than
        // guessed: CrashNavMap::UpdateButtonPrompts inlines all three of its calls as
        // `lwz r11, 0x770(r31)` (@0x824B69B4 / @0x824B6A98 / @0x824B6B84), and
        // 0x770 == mCrashNavPanel (state +0x6E0) + 0x90 == mePanelType -- not the toggle
        // group at +0xA0. The compares that follow pin the enumerators too: `cmpwi 0`
        // (E_PANEL_EVENT), `cmpwi 2` (E_PANEL_ROADSIGN), `cmpwi 3` (E_PANEL_RIVALS).
        // Non-const per the DWARF.
        PanelType GetPanelActiveFilterMode() { return mePanelType; }

        // @ 0x824BAE58 - active game/progression mode for the highlighted event. Asserts the
        // panel is showing events, then maps the embedded event panel's current game mode
        // through EventPanel::ConvertLocalEventDefToProgressionEventDef. Non-const because that
        // collaborator is a non-const method.
        BrnProgression::RaceEventData::EModeType GetPanelActiveGameModeType();

        // @ 0x824185C8 - active road-rule type. Asserts the panel is showing road rules.
        // DWARF DELTA: declared `BrnStreetData::ScoreType GetPanelActiveRoadRuleType() const`
        // (h:346). Kept as s32 here because the committed consumers compare the result
        // against plain integers; retyping it is a separate, consumer-visible change.
        // The value it returns is really mRoadPanel.meCurrentRule (RoadPanel +0xD9C,
        // an inlined RoadPanel::GetCurrentRule(), DWARF BrnRoadPanel.h:199) -- see the
        // mRoadPanel carve note in the layout.
        s32 GetPanelActiveRoadRuleType() const;

        // @ 0x82418668 - active road-rule scoring mode. Asserts the panel is showing road rules.
        // DWARF DELTA: declared
        // `BrnGui::GuiEventSetRoadRuleScoreMode::ERoadPanelModes GetRoadPanelScoreMode() const`
        // (h:363). Same reasoning as above -- left as s32. The value it returns is really
        // mRoadPanel.meCurrentScoreMode (RoadPanel +0xDA0, an inlined
        // RoadPanel::GetScoringMode(), DWARF BrnRoadPanel.h:214) -- see the mRoadPanel
        // carve note in the layout.
        s32 GetRoadPanelScoreMode() const;

    private:
        // ---- full DWARF member sequence (BrnCrashNavPanel.h:237..:264) ----------------
        // X360 console offsets are COMMENTS ONLY; the host layout is name-based (pointers
        // widen, sub-panel host sizes differ from the console spans).

        // The single member the DWARF places between the CgsGui::GuiComponent base and
        // mePanelType (see the BASE APPLIED note in the banner).
        EPrepareStage mePrepareStage;  // +0x8C  (DWARF h:237; StoreSettings' capture gate)

        PanelType     mePanelType;     // +0x90  (DWARF h:239; lwz -- the assert subject)
        PanelType     meVisiblePanel;  // +0x94  (DWARF h:240; ChangeVisiblePanelState's slot)
        GuiCache*     mpGuiCache;      // +0x98  (DWARF h:242; console 4-byte slot -- widens)

        // (console pads +0x9C..+0x9F before the toggle group)

        // DWARF h:244 spells the member type `BrnGui::MenuToggleGroup`; the X360 build
        // instantiates it as the var-size template -- the ctor @0x82500FD0 `bl`s
        // MenuToggleGroupVarSize<3>::MenuToggleGroupVarSize (@0x82500DB8) on this+0xA0,
        // and Update's vcall dispatches through the group vtable here (+0xA0).
        MenuToggleGroupVarSize<3> mFilterToggles;   // +0xA0  (console span 0xA0..0x2EA7)

        EventPanel        mEventPanel;      // +0x2EA8  (DWARF h:246; event-info sub-panel)
        DriveThruMapPanel mDrivethruPanel;  // +0x3788  (DWARF h:247)

        // ---- mRoadPanel (DWARF h:248) -- the ONE member still carried as a carve ------
        // The DWARF places `BrnGui::RoadPanel mRoadPanel` here (console +0x3A80..+0x482F,
        // 0xDB0 bytes: IconComponent base 0x94 / mRoadSign @+0xA0 (RoadSignIcon vtable
        // off_820564F0) / mRoadPanelData / mNames[4]+mScores[4]+mTargetCaption TextFields
        // @+0x2A8..+0xD0F / mBestScoreBackingAnimation @+0xD10 / meCurrentRule @+0xD9C /
        // meCurrentScoreMode @+0xDA0 / mbActive @+0xDA4 + pad). The committed
        // BrnRoadPanel.h (owned by the RoadPanel TU, NOT this one) is still its own
        // pre-DWARF partial slice whose tail fields are PRIVATE and DWARF-misnamed
        // (miSelectedFriendIndex / miScoringMode), so a typed `RoadPanel mRoadPanel` here
        // would strand this TU's two road-rule accessors. Until BrnRoadPanel.h applies its
        // DWARF shape (requested), the region stays a byte-carve with the two fields this
        // TU's accessors read split out at their console slots. Nothing is double-counted.
        u8  maRoadPanelHead[0xD9C];        // mRoadPanel head (console +0x3A80..+0x481B)
        s32 miActiveRoadRuleType;          // +0x481C == RoadPanel::meCurrentRule (+0xD9C)
        s32 miActiveRoadRuleScoreMode;     // +0x4820 == RoadPanel::meCurrentScoreMode (+0xDA0)
        u8  maRoadPanelTail[0xC];          // RoadPanel::mbActive + tail pad (+0x4824..+0x482F)

        RivalMapPanel mRivalPanel;         // +0x4830  (DWARF h:249)
        IconComponent mGenericPanel;       // +0x4FD8  (DWARF h:251; vtable off_82072F90)
        TextField     mGenericPanelText1;  // +0x506C  (DWARF h:252)
        TextField     mGenericPanelText2;  // +0x5194  (DWARF h:253)

        // The StoreSettings triple (the @0x8241872C..0x82418734 stores).
        PanelType                meSavedPanelType;   // +0x52BC  (DWARF h:262)
        EventPanel::EEventType   meSavedEventMode;   // +0x52C0  (DWARF h:263)
        BrnStreetData::ScoreType meSavedRRScoreType; // +0x52C4  (DWARF h:264; end of object,
                                                     //          console sizeof == 0x52C8)
    };
}
