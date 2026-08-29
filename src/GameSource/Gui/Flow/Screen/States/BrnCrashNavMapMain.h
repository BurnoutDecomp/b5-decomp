#pragma once

// ===================================================================================
// BrnGui::CrashNavMapMain  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnCrashNavMapMain.h
//   TU: GameSource/Gui/Flow/Screen/States/BrnCrashNavMapMain.cpp
//
// The CN_MAP_MAIN screen state: the in-game main menu / crash-nav map. It is the
// FSM node BrnGui::InGame::OpenMainMap reaches with SendStateEvent("MAP_MAIN")
// (BRNSCREENFSM node 5, and again as node 97 from ON_PAUSE), and the node whose
// input map carries GO_BACK / TOGGLE_LEFT / TOGGLE_RIGHT / the map zoom.
//
// CLASS SHAPE + MEMBER ORDER: DecFIGS DWARF
//   references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Screen/States/BrnCrashNavMapMain.h
//   -- `struct BrnGui::CrashNavMapMain : public BrnGui::CrashNavMap`, ONE own data
//   member (`bool mbFirstUpdate`, DWARF h:76), the five public virtuals and the three
//   private helpers listed below.
//
// MEMBER PLACEMENT: X360 ARTIST asm.
//   sizeof(CrashNavMap) == 24928 (pinned in BrnCrashNavMap.h). The only own-member
//   store in the whole class is `*(this + 24928)`:
//     OnEnter @0x824CCA84  `li r11,1 ; stw r11, 0x6160(r31)`   -> mbFirstUpdate = true
//     Update  @0x824DDEF0  `lbz r11, 0x6160(r31)`              -> the one-shot gate
//     Update  @0x824DDF6C  `stb r30, 0x6160(r31)` (r30 == 0)   -> cleared once the
//                                                                 cursor snap succeeds
//   0x6160 == 24928 == exactly sizeof(CrashNavMap), i.e. the first byte past the base,
//   which is what identifies it as this class's OWN member rather than a base field.
//   X360 sizeof(CrashNavMapMain) == 24944 (24928 + the byte, rounded to the base's
//   16-byte alignment) -- BrnScreenFlow::Prepare's state-size roster.
//   The host layout is NAME-BASED; the offsets above are documentation.
//
// ⚠️ RE-PARENT, DO NOT FORK. A placeholder `struct CrashNavMapMain : public
// CgsGui::State` lives in States/BrnScreenStatesLinkStubs.h with a PARTIAL
// OnEnter/OnLeave/Update in the matching .cpp (the pause wave). That declaration and
// those three bodies -- and the maiEventToObserve[19] table, which moves here -- must
// be deleted in the SAME commit that mounts this TU; two surviving declarations of
// BrnGui::CrashNavMapMain are an ODR fork. See the TU banner's DELETE-WHEN list.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"   // BrnGui::CrashNavMap (base)

namespace CgsModule { struct Event; }

namespace BrnGui
{
    // DWARF BrnCrashNavMapMain.h:41 -- struct tag per the DWARF (public inheritance).
    struct CrashNavMapMain : public CrashNavMap
    {
        // ---- lifecycle virtuals (DWARF order) ---------------------------------------
        virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm);   // @0x824B75D8 (cpp:75)
        virtual void OnEnter();                                           // @0x824CC9E8 (cpp:97)
        virtual void OnLeave();                                           // @0x824CCA98 (cpp:259)
        virtual void Update();                                            // @0x824DDDF8 (cpp:129)

        // DWARF BrnCrashNavMapMain.h:65 -- declared (and, per the .h line number, DEFINED)
        // in the original header, overriding CgsGui::State's empty default.
        //
        // ⚠️ FLAG -- X360 EVIDENCE GAP, and the answer to the wave's "does CN_MAP_EVENT
        // pin apt movie 133?" question. There is NO out-of-line body for either
        // CrashNavMapMain::GetResourcesToLoad or CrashNavMapEvent::GetResourcesToLoad in
        // the ARTIST image: scratch/func_index.tsv names GetResourcesToLoad for every
        // other CrashNav screen (Stats 0x82500008, Settings 0x82500028, Trax 0x825000E0,
        // ColourCalibrate 0x825001C0, DriverDetails 0x82500308, EnterOnlineBase
        // 0x82501170, Options 0x82508B00, AccountManagement 0x82508B20) and neither map
        // screen appears; and a repo-wide grep of .ida-exports for the table symbols
        // 0x82F26E84 / 0x82F26EBC returns exactly TWO functions -- CrashNavMap::
        // CheckForLoadComplete @0x824CB660 and OnlineGameRoomPlayerInfo::
        // UnloadMapResources @0x8249A280 -- so no third body reads the table.
        // CONCLUSION ON EVIDENCE: there is exactly ONE resource table in the family,
        // CrashNavMap::maResourcesToLoad @0x82F26E84 = {132,4}{83,4}{48,4}{70,4}{72,4}
        // {49,4}{50,4}, and it hardcodes apt movie id 132 (BrnCrashNavMapMain). There is
        // NO CN_MAP_EVENT override pinning 133; the twin screen SHARES the MapMain
        // table, which is consistent with the base constant
        // KPC_CRASHNAV_APT_MOVIE == "BrnCrashNavMapMain" (BrnCrashNavMap_wJ_03.cpp:46)
        // that CheckForLoadComplete plays for BOTH screens -- CN_MAP_EVENT switches
        // content by apt VIEW STATE, not by movie. Resource id 133
        // (BRNCRASHNAVMAPEVENT.bundle) is shipped but never requested on this build.
        // The body below is therefore the DWARF-declared shape over the base's table; it
        // is behaviour-neutral (CheckForLoadComplete calls EnsureResourcesAreLoaded on
        // the same table itself), and it is written INLINE exactly as the DWARF's .h line
        // number says the original was.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // ---- the two input virtuals this screen exists to override -------------------
        // CrashNavMap::Update dispatches KI_EVENT_CONTROLLER_INPUT_PRESSED to vtable
        // +0x24 and ..._RELEASED to +0x28; the base's own bodies are empty (no X360
        // export at 0x824B6798 / 0x824B67B8 at all), so THESE are the real input map.
        virtual void HandleCrashNavInputPressed(const CgsModule::Event* lpEvent);   // @0x824CCAE8 (cpp:282)
        virtual void HandleCrashNavInputReleased(const CgsModule::Event* lpEvent);  // @0x824CCD90 (cpp:446)

        // ---- DWARF-declared private helpers with NO X360 body ------------------------
        // [FLAG BLOCKED -- declaration only, deliberately no body]
        // The DecFIGS DWARF (a LATER, PS3 Dec-2007 build) gives these three .cpp line
        // numbers -- CheckForComponents cpp:505, SetTrackerToIcon cpp:564,
        // ToggleMapLegendDisplay cpp:635 -- so they exist in that build's source. In the
        // X360 ARTIST image they have NO export (scratch/func_index.tsv has six
        // CrashNavMapMain:: rows: Construct / OnEnter / OnLeave / Update /
        // HandleCrashNavInputPressed / HandleCrashNavInputReleased, and no seventh), and
        // none of the six bodies contains an inline expansion that can be attributed to
        // them: Update's event walk has no component-check arm, no tracker call, and the
        // legend toggle in HandleCrashNavInputPressed is the two-arm 43/44 dispatch on
        // meMapState == E_MAPSTATE_LEGEND, not a toggle. They are either later-build
        // additions or were dead-stripped here. Declared to keep the DWARF class shape
        // honest; NOT bodied, because there is nothing to reconstruct from.
        void CheckForComponents(const CgsModule::Event* lpEvent);   // DWARF cpp:505 -- no X360 body
        void SetTrackerToIcon();                                    // DWARF cpp:564 -- no X360 body
        void ToggleMapLegendDisplay();                              // DWARF cpp:635 -- no X360 body

        // ---- statics (DWARF cpp:29 / cpp:54 / cpp:56; values in the .cpp) ------------
        // ⚠️ X360-vs-DWARF DELTA, image wins: the DWARF declares
        // `const int32_t maiEventToObserve[18]` / `miNumEventsObserved = 18`. The ARTIST
        // build registers NINETEEN: OnEnter @0x824CCA0C and OnLeave @0x824CCA98 both pass
        // dword_82066358 with `li r5, 0x13` (19). The extra id is real, the table is
        // read out of .rdata, and it MOVES HERE from BrnScreenStatesLinkStubs.cpp.
        static const s32 maiEventToObserve[19];
        static const s32 miNumEventsObserved;

        // DWARF BrnCrashNavMapMain.cpp:56 -- the audio label both zoom arms pass to
        // GuiAudioTriggerEvent::Construct (X360 off_82F26EF8 -> "CodeMapZoom").
        static const char* const KPC_SOUND_MAP_ZOOM;

        // ---- data (DWARF BrnCrashNavMapMain.h:76) -----------------------------------
        // The one-shot "centre the map on the player" gate. X360 +24928 == exactly
        // sizeof(CrashNavMap); set true by OnEnter, cleared by Update once
        // CrashNavMap::PlaceCursorOnPlayer() reports the cursor placed.
        bool mbFirstUpdate;                       // X360 +24928
    };
}
