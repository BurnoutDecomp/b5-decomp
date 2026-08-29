#pragma once

// ===================================================================================
// BrnGui::CrashNavMapEvent (+ BrnGui::CrashNavMapEventKeyboardListener) -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnCrashNavMapEvent.h
//   TU: GameSource/Gui/Flow/Screen/States/BrnCrashNavMapEvent.cpp
//
// The CN_MAP_EVENT screen state: the map's event-creation twin, reached from DRIVING
// by BrnGui::InGame::OpenEventMap -> SendStateEvent("MAP_EVENT") (BRNSCREENFSM node 6)
// and gated on the player standing in an event start location. Its only exit is
// GO_BACK, sent by HandleSelect once the route is accepted.
//
// CLASS SHAPE + MEMBER ORDER: DecFIGS DWARF
//   references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Screen/States/BrnCrashNavMapEvent.h
//   -- `struct BrnGui::CrashNavMapEvent : public BrnGui::CrashNavMap`, six own data
//   members, five public virtuals + UpdatePanelData, and six private helpers.
//
// MEMBER PLACEMENT: X360 ARTIST asm, store-walked out of OnEnter @0x824CC6C0 (which
// touches every scalar member), Update @0x824DDB90, HandleSelect @0x824D8F68 and
// SetTracker @0x824BFA88. sizeof(CrashNavMap) == 24928.
//     +24928  meCreateEventStage   (OnEnter stw 0; HandleSelect stores 2/3/6;
//                                   Update asserts == 6 before reading the keyboard)
//     +24936  mCreatedRace         (BrnProgression::Race, 120 B -- Race::Construct
//                                   @0x824CC77C takes this+24936; HandleSelect's name
//                                   copy writes +24936..+24967 == BaseRace::macName[32])
//     +25056  mbUpdateNewEventInfo (OnEnter stb 0; Update's head consumes + clears it)
//     +25057  mi8NextEventIndex    (OnEnter stb 0; Update copies it to the base's
//                                   mi8CurrentEventIndex @+70)
//     +25060  mpGuiKeyboard        (OnEnter stw 0; latched from wire event 142)
//     +25064  mKeyboardListener    (CrashNavMapEventKeyboardListener, 40 B: vptr @+0,
//                                   macKeyboardString[32] @+4, mbKeyboardClosed @+36
//                                   (=+25100, OnEnter stb 0), mbNewData @+37 (=+25101))
//   X360 sizeof(CrashNavMapEvent) == 25104 (BrnScreenFlow::Prepare's state-size roster).
//   The host layout is NAME-BASED; the offsets above are documentation.
//
// ⚠️⚠️ THIS TU IS NOT MOUNTABLE YET -- see the .cpp banner. Six of the class's methods
// are BLOCKED on collaborator types that have no home in the tree, so the vtable cannot
// be completed. Until they land, this header must NOT be included by any TU in
// tools/build/build_game_exe.bat, and the placeholder
// `struct CrashNavMapEvent : public CgsGui::State` in
// States/BrnScreenStatesLinkStubs.h must STAY. Two definitions of
// BrnGui::CrashNavMapEvent in one linked image would be an ODR fork; keeping this TU
// unmounted keeps the fork dormant (only this file's own .cpp sees this declaration).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/CgsGuiKeyboard.h"           // CgsGui::GuiKeyboardListener / CgsUtf16
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"    // BrnGui::CrashNavMap (base)
#include "SharedClasses/Progression/BrnRace.h"                   // BrnProgression::Race (by value)

namespace CgsModule { struct Event; }

namespace BrnGui
{
    class BrnGuiKeyboard;   // GameSource/Gui/BrnGuiKeyboard.h (pointer member only)

    // DWARF BrnCrashNavMapEvent.h:42 -- the keyboard completion callback for the
    // "name your custom event" dialog.
    //
    // ⭐ CANONICAL HOME (DWARF), declared here 2026-08-29.
    // ⚠️ CORRECTED SAME DAY (FIX1): CgsSaveLoad.cpp does NOT include this header, and must
    // not until this TU is mounted. CgsSaveLoad.cpp is mounted and owns the KeyboardClosed
    // @0x824C1820 body; including this header from there drags in the real
    // `CrashNavMapEvent : CrashNavMap` alongside the mounted BrnScreenStatesLinkStubs.h
    // placeholder `CrashNavMapEvent : CgsGui::State` -- a LIVE ODR fork. CgsSaveLoad.cpp
    // therefore keeps a minimal, layout-identical file-local copy of THIS struct with a
    // DELETE-WHEN note pointing at the mount. Two declarations of the listener, one of
    // CrashNavMapEvent -- that is the deliberate trade until the mount.
    // The member offsets below are the ones CgsSaveLoad.cpp measured
    // (macKeyboardString @+0x04, mbKeyboardClosed @+0x24, mbNewData @+0x25), and they
    // are independently confirmed by FillString @0x824B7568, which reads +0x24/+0x25 and
    // returns `this + 4`.
    struct CrashNavMapEventKeyboardListener : public CgsGui::GuiKeyboardListener
    {
        // DWARF cpp:729 -- declaration only (no X360 export; the state's OnEnter does the
        // two byte clears inline at +25100/+25101 instead of calling it).
        void Construct();

        // @ 0x824C1820 (DWARF cpp:678). Body lives in CgsSaveLoad.cpp.
        virtual void KeyboardClosed(const CgsGui::CgsUtf16* lpResultText);

        // DWARF BrnCrashNavMapEvent.h:175 -- header-inline on the console (a .h line, no
        // out-of-line address in scratch/func_index.tsv), so its faithful home is a body
        // here over the named member.
        bool HasJustClosed() const { return mbKeyboardClosed; }

        // @ 0x824B7568 (DWARF cpp:703). Consume the dialog result: assert it really has
        // closed, clear the closed flag, and hand back the string only when the dialog
        // reported new data.
        char* FillString();

        char macKeyboardString[32];   // +0x04 .. +0x23  (DWARF h:64)
        bool mbKeyboardClosed;        // +0x24           (DWARF h:65)
        bool mbNewData;               // +0x25           (DWARF h:66)
    };

    // DWARF BrnCrashNavMapEvent.h:79 -- struct tag per the DWARF (public inheritance).
    struct CrashNavMapEvent : public CrashNavMap
    {
        // DWARF BrnCrashNavMapEvent.h:110. Values verbatim; 6 is corroborated by
        // Update's assert "meCreateEventStage == E_CREATE_EVENT_KEYBOARD" against
        // `cmpwi r11, 6` @0x824DDD1C, and 2/3 by HandleSelect's own stores.
        enum CreateEventStage
        {
            E_CREATE_EVENT_NONE          = 0,
            E_CREATE_EVENT_NEW_PANEL     = 1,
            E_CREATE_EVENT_EDIT_ROUTE    = 2,
            E_CREATE_EVENT_EDIT_MODIFIER = 3,
            E_CREATE_EVENT_SAVE          = 4,
            E_CREATE_EVENT_REPLACE       = 5,
            E_CREATE_EVENT_KEYBOARD      = 6,
            E_CREATE_EVENT_CONFIRM       = 7,
        };

        // ---- lifecycle virtuals (DWARF order) ---------------------------------------
        virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm);   // @0x824B7510 (cpp:59)
        virtual void OnEnter();                                           // @0x824CC6C0 (cpp:76)
        virtual void OnLeave();                                           // @0x824CC790 (cpp:232)
        virtual void Update();                                            // @0x824DDB90 (cpp:118) [BLOCKED]

        // DWARF BrnCrashNavMapEvent.h:103 -- header-inline, same shape and the same
        // X360 evidence gap as CrashNavMapMain's (see that header's long FLAG). SHORT
        // FORM: the ARTIST image has NO out-of-line GetResourcesToLoad for either map
        // screen, and the ONLY two functions in the whole export set that touch the
        // resource table @0x82F26E84 are CrashNavMap::CheckForLoadComplete and
        // OnlineGameRoomPlayerInfo::UnloadMapResources. There is therefore NO
        // CN_MAP_EVENT-specific table and NOTHING pins apt resource id 133
        // (BrnCrashNavMapEvent): this screen shares CrashNavMap::maResourcesToLoad,
        // whose first tuple is {132, E_GUI_RESOURCETYPE_APT} == BrnCrashNavMapMain --
        // consistent with the base constant KPC_CRASHNAV_APT_MOVIE ==
        // "BrnCrashNavMapMain" (BrnCrashNavMap_wJ_03.cpp:46) that CheckForLoadComplete
        // plays for BOTH screens. CN_MAP_EVENT differentiates by apt VIEW STATE, not by
        // movie. Answered on evidence; FLAGged because the absence of a body is the
        // evidence.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // DWARF BrnCrashNavMapEvent.h (cpp:664) -- vtable slot the console calls as
        // `(*(*this + 52))(this)` from Update's head @0x824DDBE0. [BLOCKED]
        virtual void UpdatePanelData();

        // ---- private helpers (DWARF order) -------------------------------------------
        void HandleControllerInput(const CgsModule::Event* lpEvent);   // DWARF cpp:257 [BLOCKED]
        void HandleSelect();                                           // @0x824D8F68 (cpp:443) [BLOCKED]
        void SetTracker();                                             // @0x824BFA88 (cpp:526) [BLOCKED]
        void UpdateEventData();                                        // DWARF cpp:590 [BLOCKED]
        void ClearTracker();                                           // @0x824BCD38 (cpp:607)
        void SetEventData();                                           // @0x824CC830 (cpp:627) [BLOCKED]

        // ---- statics (DWARF cpp:26 / cpp:41) -----------------------------------------
        // X360 dword_8206632C; OnEnter @0x824CC6EC and OnLeave @0x824CC7A0 both pass it
        // with `li r5, 0xA` (10), which matches the DWARF's own `[10]` / `= 10`.
        static const s32 maiEventToObserve[10];
        static const s32 miNumEventsObserved;

        // ---- data members (DWARF order; X360 offsets in the header banner) -----------
        CreateEventStage meCreateEventStage;      // X360 +24928
        BrnProgression::Race mCreatedRace;        // X360 +24936 (120 B)
        bool             mbUpdateNewEventInfo;    // X360 +25056
        s8               mi8NextEventIndex;       // X360 +25057
        BrnGuiKeyboard*  mpGuiKeyboard;           // X360 +25060
        CrashNavMapEventKeyboardListener mKeyboardListener;   // X360 +25064
    };
}
