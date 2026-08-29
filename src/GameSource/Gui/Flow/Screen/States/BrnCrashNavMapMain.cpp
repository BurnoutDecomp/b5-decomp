// ===================================================================================
// BrnGui::CrashNavMapMain -- the CN_MAP_MAIN screen state (the in-game main menu).
//
//   Construct                  @0x824B75D8   (BrnCrashNavMapMain.cpp:75)
//   OnEnter                    @0x824CC9E8   (cpp:97)
//   Update                     @0x824DDDF8   (cpp:129)
//   OnLeave                    @0x824CCA98   (cpp:259)
//   HandleCrashNavInputPressed @0x824CCAE8   (cpp:282)
//   HandleCrashNavInputReleased@0x824CCD90   (cpp:446)
//
// All six bodies are read off the raw X360 ARTIST assembly
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/<addr>.json "assembly" field), with Hex-Rays
// used only as a cross-check -- its rendering of this class is unusually poor (it
// prints the `this` pointer as `_DWORD *result` and indexes members as word offsets:
// result[14] is +56 meMapState, result[476] is +1904 == mCrashNavPanel.mePanelType,
// result[6100] is +24400 meCursorMode, result[6179]/[6181]/[6183] are +24716 /
// +24724 / +24732).
//
// ⭐ SUPERSEDES the pause-wave PARTIAL in BrnScreenStatesLinkStubs.cpp:72-200. That
// partial got the pause right and said so honestly; everything it deliberately parked
// is landed here:
//   * `*(this+24928) = 1` (mbFirstUpdate) and `*(this+56) = 1` (meMapState =
//     E_MAPSTATE_MAP) in OnEnter -- both land past sizeof(CgsGui::State) and were
//     unwritable while the placeholder derived from State. With the real CrashNavMap
//     base they are ordinary member stores.
//   * OnLeave's CrashNavPanel::StoreSettings(mCrashNavPanel, false) + CrashNavMap::OnLeave.
//   * Update's chain to CrashNavMap::Update (which is what actually dispatches
//     controller input to HandleCrashNavInputPressed below -- the partial drained the
//     in-queue itself because the base spine did not exist), the first-update cursor
//     snap, and the in-event landmark re-latch.
//   * SEVEN of the nine input arms: the partial implemented 45|50 only.
//
// ⛔ DELETE-WHEN, PER SYMBOL (all in the conductor-owned link-stub TU; they must go in
// the SAME commit that mounts this file or the link is LNK2005):
//   BrnScreenStatesLinkStubs.h  -- `struct CrashNavMapMain : public CgsGui::State`
//                                  (the whole declaration, incl. its two static decls)
//   BrnScreenStatesLinkStubs.cpp-- CrashNavMapMain::maiEventToObserve[19]
//                                  CrashNavMapMain::miNumEventsObserved
//                                  CrashNavMapMain::OnEnter
//                                  CrashNavMapMain::OnLeave
//                                  CrashNavMapMain::Update
//
// LINK-TIME EXTERNALS this TU needs and does not define (cl /c cannot see them;
// reported, not fabricated). Every one of them is a real X360 body that belongs to a
// TU this wave is mounting alongside:
//   CrashNavMap::{Construct, OnEnter, OnLeave, Update, UpdateButtonPrompts,
//                 PlaceCursorOnPlayer}   (BrnCrashNavMap_wJ_0*.cpp + the still-missing
//                                         BrnCrashNavMap.cpp -- OnLeave @0x824CB440 and
//                                         PlaceCursorOnPlayer @0x824BF6F0 live there)
//   CrashNavMap::maResourcesToLoad / muNumResourcesToLoad  (same TU; the header-inline
//                                         GetResourcesToLoad above odr-uses them)
//   CrashNavLegend::{HighlightNext, HighlightPrevious}     (BrnCrashNavLegend.cpp)
//   CrashNavPanel::{ToggleRoadPanelScores, GetRoadPanelScoreMode,
//                   IsRoadRuleFriendSelected, GetRoadRuleFriendSelectedName,
//                   StoreSettings}                        (BrnCrashNavPanel.cpp)
//   MainMapComponent::{SetZoom, SnapToLocation}            (BrnMainMap.cpp / LinkGates)
//   GuiAudioTriggerEvent::Construct                        (BrnGuiEventTypeDefs.cpp)
//   GuiCache::{GetWorldCameraPosition, GetProfileEventDisplayInfo,
//              HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID}
//   MainMapCacheBoundary::IsHighDef                        (BrnMainMap.cpp, MOUNTED)
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMapMain.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent / GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface, GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / VariableEventQueue
#include "GameSource/GameState/BrnCgsPlayerName.h"                        // CgsNetwork::PlayerName
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // GuiEventSetInspectedEventIcon / ...GamercardEvent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventActivateCrashNav / GuiAudioTriggerEvent
#include "GameSource/Gui/SatNav/BrnMainMap.h"                             // MainMapComponent::SetZoom / SnapToLocation
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                      // BrnGui::MapIconManager

#include <cstring>   // std::memcpy (the id-457 audio re-post + the inspected-icon payload)

namespace BrnGui
{
    // MainMapComponent::Construct's GuiCache high-definition boundary, defined in the
    // MOUNTED GameSource/Gui/SatNav/BrnMainMap.cpp:163. Declared here rather than
    // re-implemented: the cache byte at X360 GuiCache+0x4B49 still falls inside
    // BrnGuiCache.h's `mPad_4B44[6]`, this TU may not carve the cache header, and a
    // second local stand-in would be a fork of a single-definition boundary. The zoom
    // arm below is its FOURTH attested consumer (BrnMainMap.cpp:131 already lists it).
    // DELETE-WHEN BrnGuiCache.h carves that byte as `bool mbIsHighDef; // +0x4B49` with
    // an accessor -- then this reads mpGuiCache->IsHighDef() and the declaration goes.
    namespace MainMapCacheBoundary { bool IsHighDef(const GuiCache* lpGuiCache); }

    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) ------------------------
        // Same legend as BrnCrashNavMap_wJ_08.cpp: 40 == OutputGuiEvent, 41 ==
        // OutputViewState, 42 == OutputInternalState.
        const s32 KI_CHANNEL_GUI_EVENT = 40;

        // ---- the state input queue --------------------------------------------------
        // CgsGui::State::mpInGuiEventQueue is an opaque InputBuffer::GuiEventQueue*; the
        // X360 calls VariableEventQueue<18432,16>::GetFirstEvent / GetNextEvent / Clear
        // on it (`lwz r3, 0x18(r31)` == mpInGuiEventQueue). Same typedef +
        // reinterpret_cast as BrnCrashNavMap_wJ_08.cpp and every other committed GUI state.
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- the wire ids Update's own walk dispatches on ---------------------------
        // Read from the compare chain at 0x824DDE3C..0x824DDE58. All three are members
        // of maiEventToObserve below, so they are ids this state is registered for.
        // FLAG consumer-named: 43 / 44 have no recovered type name; the only thing the
        // arm does is repaint the button prompts, and they sit next to the legend's
        // 43/44 ACTION ids in the same table, so the naming is deliberately neutral.
        const s32 KI_EVENT_MAP_PROMPTS_A = 43;
        const s32 KI_EVENT_MAP_PROMPTS_B = 44;
        const s32 KI_EVENT_UI_VISIBLE    = 516;   // 0x204

        // ---- the GUI ACTION ids HandleCrashNavInput{Pressed,Released} switch on ------
        // The X360 switch is `action - 0x2B` with a 15-entry jump table
        // (0x824CCAFC..0x824CCB1C), so the ids are 43..57. Names are the DWARF
        // EGameInputActions spellings (references/DecFIGS/dwarfdump/GameSource/Input/
        // GameInputActions.h:24) -- see scratch/mainmenu_wave/s3_input.md §1.1.
        const s32 KI_ACTION_GUI_LEFT      = 43;
        const s32 KI_ACTION_GUI_RIGHT     = 44;
        const s32 KI_ACTION_GUI_START     = 45;
        const s32 KI_ACTION_GUI_SELECT    = 49;
        const s32 KI_ACTION_GUI_CANCEL    = 50;
        const s32 KI_ACTION_GUI_OPTION0   = 51;
        const s32 KI_ACTION_GUI_LSHOULDER = 54;
        const s32 KI_ACTION_GUI_RSHOULDER = 55;
        const s32 KI_ACTION_GUI_LTRIGGER  = 56;
        const s32 KI_ACTION_GUI_RTRIGGER  = 57;

        // ---- the FSM script events this screen sends --------------------------------
        // X360 rodata aGoBack / aToggleLeft / aToggleRight.
        const char KAC_STATE_EVENT_GO_BACK[]      = "GO_BACK";
        const char KAC_STATE_EVENT_TOGGLE_LEFT[]  = "TOGGLE_LEFT";
        const char KAC_STATE_EVENT_TOGGLE_RIGHT[] = "TOGGLE_RIGHT";

        // ---- the two custom map-zoom distances --------------------------------------
        // X360 flt_82066264 / flt_82066268, selected by the GuiCache high-definition
        // byte: SET -> 9000, CLEAR -> 12000 (0x824CCC74..0x824CCCA0).
        const f32 KF_MAP_ZOOM_OUT_HD = 9000.0f;
        const f32 KF_MAP_ZOOM_OUT_SD = 12000.0f;

        // X360 flt_82001CC0 == 0.0f -- the custom-zoom argument the RELEASE arms pass
        // alongside E_ZOOMFACTOR_MEDIUM (ignored by SetZoom for a non-custom factor, but
        // the console loads and passes it, so it is reproduced).
        const f32 KF_MAP_ZOOM_UNUSED_CUSTOM = 0.0f;

        // The GuiAudioTriggerEvent action codes the two zoom arms pass (r4 == 8 on
        // press @0x824CCCB0, r4 == 9 on release @0x824CCE5C). The component and movie
        // arguments are both the empty rodata string &unk_820046A7.
        const s32 KI_AUDIO_ACTION_MAP_ZOOM_IN  = 8;
        const s32 KI_AUDIO_ACTION_MAP_ZOOM_OUT = 9;
        const char KAC_EMPTY_STRING[] = "";                      // X360 &unk_820046A7

        // ---- GsmIO::EGameModeType values Update tests -------------------------------
        // FLAG: raw values, exactly as the X360 encodes them (`cmpwi r10,2` /
        // `cmpwi r10,0x10` @0x824DDF9C..0x824DDFB0 for the "not this mode" pair, and
        // `cmpwi r11,0` / `,5` / `,8` @0x824DDFE8..0x824DE000 for the in-event triple).
        // BrnGuiCache.h routes the word through GetGameMode() and documents it as a
        // BrnGameState::GameStateModuleIO::EGameModeType whose enum header this GUI TU
        // deliberately does not pull in (the same convention GetCurrentGameModeType uses).
        const s32 KI_GAMEMODE_EXCLUDE_A = 2;
        const s32 KI_GAMEMODE_EXCLUDE_B = 16;
        const s32 KI_GAMEMODE_ROUTE_A   = 0;
        const s32 KI_GAMEMODE_ROUTE_B   = 5;
        const s32 KI_GAMEMODE_ROUTE_C   = 8;

        // The animation parameter the in-event landmark re-latch passes (`lfs f1,
        // flt_82065670`-class 1.0f in f1 @0x824DE01C, with r6 == 0 -- the PPC float-arg
        // GPR skip is why Hex-Rays loses the trailing bool).
        const f32 KF_ACTIVE_LANDMARKS_T = 1.0f;

        // The X360 assert-site file string, verbatim (aGamesourceGuiF_xx).
        const char KAC_ASSERT_FILE[] =
            "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCrashNavMapMain.cpp";

        // ---- in-queue payload view --------------------------------------------------
        // Wire id 516. The queue hands the state the HEADER-STRIPPED payload and the
        // console reads its leading word (`lwz r11, 0(r21)` / `cmpwi r11, 1`
        // @0x824DDE68). FLAG consumer-named: only the leading word has a recovered role,
        // and the assert literal ("lpUiVisibleEvent != NULL") is where the name comes from.
        struct UiVisiblePayload : public CgsModule::Event
        {
            u32 muVisible;   // +0x00  the console acts only on the exact value 1
        };

        // ---- out-queue payload view: the id-457 audio re-post ------------------------
        // HandleCrashNavInputReleased's zoom arm builds a normal GuiAudioTriggerEvent,
        // then memcpy's its 100-BYTE PAYLOAD into a wrapper record whose type word is
        // 457, not 201 (@0x824CCE78..0x824CCEB4: `memcpy(dst, &audio, 0x64)` then the
        // three header words 100 / 0x1C9 / 12 and `AddEvent(out+12, rec, 40, 0x70)`).
        // 0x70 == 112 == 12 + 100, which is exactly GuiEventWrapper<T,40> over a
        // 100-byte T. The same 100-byte-payload-under-id-457 record is already attested
        // in the tree at BrnInGameMessageRenderer.cpp:1347/1413 ("CodeTicker"), and
        // BrnGuiDemangledEventTypes.h:472 FLAGs the 201-vs-457 split as unresolved --
        // this view is the honest local shape, not a second home for the type.
        struct GuiAudioTriggerPayload457
        {
            char macComponent[32];   // +0x00
            s32  meAction;           // +0x20
            char macLabel[32];       // +0x24
            char macMovie[32];       // +0x44
            s32 GetEventType() const { return 457; }
        };
        // 100 == the console's memcpy size and the record's leading size word.
        // (The C++ struct is naturally 4-aligned, so no padding creeps in.)

        // ---- the zoomed-out map centre ----------------------------------------------
        // X360 &unk_82FB4C20 -- a 16-byte .bss quadword the LTRIGGER arm loads whole and
        // stores into mMainMapComponent.mv2DesiredCentre. ⚠️ FLAG, and it is the exact
        // shape of a trap this project has hit before: A .BSS ZERO DOES NOT PROVE THE
        // CONSOLE VALUE IS ZERO (dynamic initialisers have no IDA export). What IS
        // measured: a repo-wide grep of .ida-exports for 0x82FB4C20 returns exactly TWO
        // functions, and both are READERS with identical expansions --
        // CrashNavMapMain::HandleCrashNavInputPressed @0x824CCCA4 and
        // OnlineGameRoomPlayerInfo::HandleControllerInputPressedMapSubState @0x824A5240.
        // No writer exists in the export set, so the object is either zero-initialised or
        // filled by an unexported dyn-init. Modelled as the all-zero Vector2 (the world
        // origin, which is what "pull the map all the way out and centre it" wants and
        // what a `static const Vector2 KV2_...(0,0)` compiles to in .bss), returned from
        // a function so the constant has one definition and one FLAG.
        Vector2 KV2_ZOOMED_OUT_MAP_CENTRE()
        {
            Vector2 lv2Centre;
            lv2Centre.x = 0.0f;
            lv2Centre.y = 0.0f;
            lv2Centre.z = 0.0f;
            lv2Centre.w = 0.0f;
            return lv2Centre;
        }

        // ---- the exit sequence -------------------------------------------------------
        // The console emits these four steps INLINE at BOTH sites -- the 516 arm of
        // Update (@0x824DDE70..0x824DDEE0) and cases 45 / 50 of
        // HandleCrashNavInputPressed (@0x824CCB8C..0x824CCBF0) -- and the two expansions
        // are byte-identical: resume the network, ACTIVATE crash-nav (this is the
        // UNPAUSE: GameBridgeGUIToX_GameState turns id 191 payload 1 into game event 93
        // -> RequestPause(4) -> mbSimPaused = 0), the { 1, 533, 12 } acknowledgement,
        // then the FSM hand-off back to INGAME. Factored to one function purely so the
        // two call sites cannot drift; nothing about the emitted records changes.
        void PostExitToInGame(CgsGui::StateInterface* lpStateInterface)
        {
            CgsGui::GuiEventNetworkSuspension lResume(false);              // { 4, 45, 12, 0 } ch40 16
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lResume), KI_CHANNEL_GUI_EVENT,
                static_cast<s32>(sizeof(lResume)));

            GuiEventActivateCrashNav lActivate(true);                      // { 8, 191, 12, 1, 0 } ch40 20
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lActivate), KI_CHANNEL_GUI_EVENT,
                static_cast<s32>(sizeof(lActivate)));

            CgsGui::GuiEvent<533> lDone(1, 12);                            // { 1, 533, 12 } ch40 16
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lDone), KI_CHANNEL_GUI_EVENT, 16);
        }
    }

    // ---------------------------------------------------------------------------------
    // The 19 GUI event ids OnEnter/OnLeave (un)register, read out of .rdata at
    // dword_82066358 -- the exact pointer both @0x824CCA0C and @0x824CCA98 hand to
    // (Un)RegisterForEvents with `li r5, 0x13`. Id 6 == KI_EVENT_CONTROLLER_INPUT_PRESSED
    // is first and is what makes the input map below reachable at all.
    // MOVED HERE from BrnScreenStatesLinkStubs.cpp (pause wave) unchanged.
    // ---------------------------------------------------------------------------------
    const s32 CrashNavMapMain::maiEventToObserve[19] =
    {
        6, 7, 8, 14, 16, 43, 44, 202, 224, 213, 199, 64, 436, 334, 516, 438, 189, 344, 332
    };
    const s32 CrashNavMapMain::miNumEventsObserved = 19;

    // X360 off_82F26EF8 -- the audio label both zoom arms pass (IDA renders the string
    // inline at the load: `lwz r6, off_82F26EF8@l(r11)  # "CodeMapZoom"`). The DWARF
    // names the constant KPC_SOUND_MAP_ZOOM (BrnCrashNavMapMain.cpp:56).
    const char* const CrashNavMapMain::KPC_SOUND_MAP_ZOOM = "CodeMapZoom";

    // =================================================================================
    //  Construct  @0x824B75D8  (cpp:75)
    //
    //  Chain to the base and then undo two of its cold-start choices: the main map does
    //  NOT draw road signs and does NOT show drive-throughs.
    // =================================================================================
    void CrashNavMapMain::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
    {
        CGS_ASSERT(lpFsm != 0, "lpFsm");                                   // cpp:78

        CrashNavMap::Construct(liId, lpFsm);

        // 0x824B7614 / 0x824B7618 -- `stb r11(=0), 0x6081(r3)` and `stb r11, 0x6084(r3)`.
        // 0x6081 == 24705 == mbUseRoadSigns; 0x6084 == 24708 == meEventIconDisplayType.
        // Note the SECOND store is the icon-display TYPE word, not the neighbouring
        // drive-through byte: the base's Construct already stores mbDrawDriveThrus TRUE
        // at 0x6082 and this override does not touch it (BrnCrashNavMap_wJ_04.cpp's own
        // banner records that offset triple).
        mbUseRoadSigns         = false;
        meEventIconDisplayType = 0;   // GuiEventDrawEventIcons::EIconDisplayType 0
    }

    // =================================================================================
    //  OnEnter  @0x824CC9E8  (cpp:97)
    //
    //  Bring the map screen up: chain the base, subscribe to the 19 events, tell the
    //  game to DEACTIVATE crash-nav and SUSPEND the network -- which together are the
    //  offline pause -- then arm the one-shot player snap and open on the MAP page.
    // =================================================================================
    void CrashNavMapMain::OnEnter()
    {
        CrashNavMap::OnEnter();

        // 0x824CCA0C -- REGISTER FIRST. A state that observes nothing receives nothing,
        // so every input arm below is dead code until this runs (measured the hard way
        // during the pause wave: the map paused one-way).
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // 0x824CCA1C..0x824CCA40 -- { 8, 191, 12, 0, 0 } ch 40, 20 bytes.
        GuiEventActivateCrashNav lDeactivate(false);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lDeactivate), KI_CHANNEL_GUI_EVENT,
            static_cast<s32>(sizeof(lDeactivate)));

        // 0x824CCA44..0x824CCA70 -- { 4, 45, 12, 1 } ch 40, 16 bytes. Id 45 ==
        // CgsGui::GuiEventNetworkSuspension; the payload word is the suspend flag.
        CgsGui::GuiEventNetworkSuspension lSuspend(true);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lSuspend), KI_CHANNEL_GUI_EVENT,
            static_cast<s32>(sizeof(lSuspend)));

        // 0x824CCA84 / 0x824CCA88 -- the two stores the pause-wave partial could not
        // make. +24928 is this class's own byte; +56 is the base's meMapState.
        mbFirstUpdate = true;
        meMapState    = E_MAPSTATE_MAP;
    }

    // =================================================================================
    //  OnLeave  @0x824CCA98  (cpp:259)
    //
    //  Release the event subscription (the observer table is only
    //  CgsGui::KI_MAX_OBSERVERS == 4 slots wide, so this is not optional bookkeeping),
    //  capture the panel's live filter/score settings for the next visit, then chain
    //  the base's teardown.
    // =================================================================================
    void CrashNavMapMain::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // 0x824CCAC0 -- `addi r3, r31, 0x6E0 ; li r4, 0 ; bl StoreSettings`. FALSE is the
        // CAPTURE flavour (BrnCrashNavPanel.h's measured note: TRUE writes the
        // K_DEFAULT_* triple instead). 0x6E0 == 1760 == mCrashNavPanel.
        mCrashNavPanel.StoreSettings(false);

        CrashNavMap::OnLeave();
    }

    // =================================================================================
    //  Update  @0x824DDDF8  (cpp:129)
    //
    //  The per-frame pump. Runs the base spine first -- which is what dispatches
    //  controller input into HandleCrashNavInput{Pressed,Released} below -- then does
    //  this screen's own three jobs: a second walk of the in-queue for the button-prompt
    //  refresh and the "UI became visible again" exit, the one-shot snap that centres the
    //  map on the player and grows the icons, and the in-event route re-latch that keeps
    //  the active landmark set pointed at the event the player is currently running.
    //  Finishes by clearing the in-queue (the base does not).
    // =================================================================================
    void CrashNavMapMain::Update()
    {
        CrashNavMap::Update();

        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liEventSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
        {
            if (liEventId == KI_EVENT_MAP_PROMPTS_A || liEventId == KI_EVENT_MAP_PROMPTS_B)
            {
                CrashNavMap::UpdateButtonPrompts();
            }
            else if (liEventId == KI_EVENT_UI_VISIBLE)
            {
                // cpp:151 -- non-fatal on the X360; the payload is dereferenced either way.
                CGS_ASSERT(lpEvent != 0, "lpUiVisibleEvent != NULL");

                const UiVisiblePayload* lpUiVisibleEvent =
                    static_cast<const UiVisiblePayload*>(lpEvent);

                // `cmpwi r11, 1` -- an explicit compare against 1, not a zero test.
                if (lpUiVisibleEvent->muVisible == 1)
                {
                    PostExitToInGame(mpStateInterface);
                    SendStateEvent(KAC_STATE_EVENT_GO_BACK);   // CN_MAP_MAIN(5) -> INGAME(4)
                }
            }
        }

        // ---- the one-shot player snap (0x824DDEF0..0x824DDF74) -----------------------
        if (mbFirstUpdate)
        {
            if (mpGuiCache != 0)
            {
                // The console takes the address of the cache's player-info block at
                // +0x4AE0 and asserts it -- an address-of, so the test is structurally
                // always true; reproduced because the console reproduces it. The block's
                // head is the world camera/player position lane, which is what
                // GuiCache::GetWorldCameraPosition() exposes.
                const Vector4* lpPlayerInfo = &mpGuiCache->GetWorldCameraPosition();
                CGS_ASSERT(lpPlayerInfo != 0, "lpPlayerInfo");             // cpp:205

                // The `lvx128` + `vperm` against unk_82CDA450
                // (= 00010203 18191A1B 00010203 00010203) flattens the world (X,Y,Z,W)
                // to the 2-D map point (X,Z,X,X). Whole-quadword, so all four lanes are
                // committed -- same expansion as OnlineGameRoomPlayerInfo_wH_00.cpp:476.
                Vector2 lv2MapLocation;
                lv2MapLocation.x = lpPlayerInfo->x;
                lv2MapLocation.y = lpPlayerInfo->z;
                lv2MapLocation.z = lpPlayerInfo->x;
                lv2MapLocation.w = lpPlayerInfo->x;
                mMainMapComponent.SnapToLocation(lv2MapLocation);

                if (CrashNavMap::PlaceCursorOnPlayer())
                {
                    // `stwx 1, iconmgr, 0xAA04` -- MapIconManager::meIconSizeMode.
                    mpIconManager->meIconSizeMode = MapIconManager::E_ICONSIZE_LARGE;
                    mbFirstUpdate = false;
                }
            }
        }

        // ---- the in-event gate (0x824DDF78..0x824DDFD4) ------------------------------
        // Three successive stores into the SAME byte, in the console's order: the cache's
        // in-event colouring gate, ANDed with "the game mode is neither 2 nor 16", ANDed
        // with "no online start is in progress". Written as three assignments rather than
        // one expression because that is literally what the asm does (`stb` x3).
        if (mpGuiCache != 0)
        {
            const bool lbInEventColouring = mpGuiCache->GetInEventColouringGate();
            mbIsInEvent = lbInEventColouring;

            const s32 leGameMode = mpGuiCache->GetGameMode();
            const bool lbExcludedMode =
                (leGameMode == KI_GAMEMODE_EXCLUDE_A || leGameMode == KI_GAMEMODE_EXCLUDE_B);
            mbIsInEvent = (!lbExcludedMode) && lbInEventColouring;

            mbIsInEvent = (!mpGuiCache->IsOnlineStartInProgress()) && mbIsInEvent;
        }

        // ---- the in-event route re-latch (0x824DDFD8..0x824DE024) --------------------
        // NOTE: the console does NOT re-null-check the cache here -- it reuses the
        // register it loaded above. Reproduced as written; mbIsInEvent can only be true
        // if the block above ran, which required a non-null cache.
        if (mbIsInEvent)
        {
            const s32 leGameMode = mpGuiCache->GetGameMode();
            if (leGameMode == KI_GAMEMODE_ROUTE_A ||
                leGameMode == KI_GAMEMODE_ROUTE_B ||
                leGameMode == KI_GAMEMODE_ROUTE_C)
            {
                // `stbx 1, iconmgr, 0xAA21` / `stwx 0, iconmgr, 0xAA14` /
                // `stwx <junction>, iconmgr, 0xAA10`.
                mpIconManager->mbShowingCrashNavRoute = true;
                mpIconManager->miSelectedCheckpoint   = 0;

                // `bl sub_824F8AF0` == GuiCache::GetProfileEventDisplayInfo (the +0x18
                // event-instance matcher); the console then lifts the record's +0x14 word,
                // which BrnGuiCache.h names muJunctionId off exactly this call pair.
                const SatNavEventDisplayInfo* lpEventStart =
                    mpGuiCache->GetProfileEventDisplayInfo(mpGuiCache->GetEventID());
                mpIconManager->muSelectedJunctionID = lpEventStart->muJunctionId;

                mpGuiCache->HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID(
                    mpGuiCache->GetEventID(), KF_ACTIVE_LANDMARKS_T, false);
            }
        }

        lpInQueue->Clear();
    }

    // =================================================================================
    //  HandleCrashNavInputPressed  @0x824CCAE8  (cpp:282)
    //
    //  The screen's whole input map -- nine arms over a 15-entry jump table based at
    //  action 43. This is the vtable +0x24 slot CrashNavMap::Update dispatches
    //  KI_EVENT_CONTROLLER_INPUT_PRESSED to; the action id is the payload word at +4,
    //  the same layout InGame::HandleControllerInput reads.
    // =================================================================================
    void CrashNavMapMain::HandleCrashNavInputPressed(const CgsModule::Event* lpEvent)
    {
        // `lwz r11, 4(r4)` -- the header-stripped payload's action sub-id.
        const s32 liAction =
            *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4);

        switch (liAction)
        {
        case KI_ACTION_GUI_LEFT:      // 43 -- legend previous
            // `lwz r11, 0x38(r31) ; cmpwi 2` -- +56 is meMapState, 2 is E_MAPSTATE_LEGEND.
            if (meMapState == E_MAPSTATE_LEGEND)
            {
                mCrashNavLegend.HighlightPrevious();   // this + 0x59B0 == 22960
            }
            break;

        case KI_ACTION_GUI_RIGHT:     // 44 -- legend next
            if (meMapState == E_MAPSTATE_LEGEND)
            {
                mCrashNavLegend.HighlightNext();
            }
            break;

        case KI_ACTION_GUI_START:     // 45
        case KI_ACTION_GUI_CANCEL:    // 50 -- both exit the map and resume the world
            PostExitToInGame(mpStateInterface);
            SendStateEvent(KAC_STATE_EVENT_GO_BACK);
            break;

        case KI_ACTION_GUI_SELECT:    // 49 -- flip the road panel between the score modes
            // `clrlwi r11, r3, 24 ; cmplwi 1` -- an explicit compare against 1 on the
            // returned byte, not a truthiness test.
            if (mCrashNavPanel.ToggleRoadPanelScores())
            {
                CrashNavMap::UpdateButtonPrompts();
            }
            break;

        case KI_ACTION_GUI_OPTION0:   // 51 -- gamercard for the highlighted road-rule friend
            // Five nested gates, in the console's order (0x824CCD18..0x824CCD84):
            //   mCrashNavPanel.mePanelType == E_PANEL_ROADSIGN   (`lwz 0x770` == 1760+0x90)
            //   mpLockedIconName != 0                            (`lwz 0x608C` == 24716)
            //   GetRoadPanelScoreMode() == 1
            //   mpGuiCache->AreRoadRuleFriendScoresAvailable()   (`lbz 0x4B50(cache)`)
            //   IsRoadRuleFriendSelected()
            if (mCrashNavPanel.GetPanelActiveFilterMode() == CrashNavPanel::E_PANEL_ROADSIGN &&
                mpLockedIconName != 0 &&
                mCrashNavPanel.GetRoadPanelScoreMode() == 1 &&
                mpGuiCache->AreRoadRuleFriendScoresAvailable() &&
                mCrashNavPanel.IsRoadRuleFriendSelected())
            {
                // The console builds a bare CgsNetwork::PlayerName on the stack and hands
                // it to OutputGuiEvent<GuiEventScoreboardRequestGamercardEvent>, whose
                // X360 instantiation @0x82493D38 emits { 16, 120, 12, name[16] } ch40, 28
                // bytes. The committed PC type bakes those three header words into its
                // GuiEvent<120> base, so filling its mPlayerName reproduces the record.
                GuiEventScoreboardRequestGamercardEvent lRequest;
                lRequest.mPlayerName.Construct(mCrashNavPanel.GetRoadRuleFriendSelectedName());
                mpStateInterface->OutputGuiEvent<GuiEventScoreboardRequestGamercardEvent>(lRequest);
            }
            break;

        case KI_ACTION_GUI_LSHOULDER: // 54 -- tab left across the CrashNav screens
            SendStateEvent(KAC_STATE_EVENT_TOGGLE_LEFT);
            break;

        case KI_ACTION_GUI_RSHOULDER: // 55 -- tab right
            SendStateEvent(KAC_STATE_EVENT_TOGGLE_RIGHT);
            break;

        case KI_ACTION_GUI_LTRIGGER:  // 56 -- hold to zoom the map out
            if (mpGuiCache != 0)
            {
                // `lbz r11, 0x4B49(cache)` -- the cache high-definition byte. SET picks
                // the 9000 pull-back, CLEAR the 12000 one. Routed through the single
                // committed boundary (see the declaration at the top of this file).
                const f32 lfCustomZoom = MainMapCacheBoundary::IsHighDef(mpGuiCache)
                                             ? KF_MAP_ZOOM_OUT_HD
                                             : KF_MAP_ZOOM_OUT_SD;

                // `li r4, 3 ; li r6, 0` -- custom factor, do NOT apply immediately (the
                // map animates to it).
                mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_CUSTOM,
                                          lfCustomZoom, false);

                // `lvx128 v0, &unk_82FB4C20 ; stvx128 v0, r31, 0x6B0` -- 0x6B0 == 1712 ==
                // mMainMapComponent(+96) + mv2DesiredCentre(+1616), i.e. the inlined
                // MainMapComponent::SetDesiredWorldCentre.
                mMainMapComponent.SetDesiredWorldCentre(KV2_ZOOMED_OUT_MAP_CENTRE());

                GuiAudioTriggerEvent lAudio;
                lAudio.Construct(KI_AUDIO_ACTION_MAP_ZOOM_IN, KAC_EMPTY_STRING,
                                 KPC_SOUND_MAP_ZOOM, KAC_EMPTY_STRING);
                mpStateInterface->OutputGuiEvent<GuiAudioTriggerEvent>(lAudio);

                // `li r11, 3 ; stw r11, 0x5F50(r31)` -- +24400 meCursorMode.
                meCursorMode = E_CURSORMODE_ZOOMEDOUT;
            }
            break;

        case KI_ACTION_GUI_RTRIGGER:  // 57 -- inspect the hovered event icon
            {
                // `lwz r11, 0x6094(r31)` (+24724 muHoveredEventID) -> the stack payload
                // AND `stw r11, 0x609C(r31)` (+24732 muInspectingEventID).
                const u32 luEventId = muHoveredEventID;
                muInspectingEventID = luEventId;

                // Wire id 558, 4-byte payload, posted on BOTH the view-state (41) and the
                // internal-state (42) channels from the same stack word.
                GuiEventSetInspectedEventIcon lInspected;
                std::memcpy(lInspected.maData, &luEventId, sizeof(luEventId));
                mpStateInterface->OutputViewState<GuiEventSetInspectedEventIcon>(lInspected);
                mpStateInterface->OutputInternalState<GuiEventSetInspectedEventIcon>(lInspected);

                meCursorMode = E_CURSORMODE_INSPECTING_ICONS;   // `li r11, 2 ; stw 0x5F50`
            }
            break;

        default:
            // The jump table's default covers 46/47/48 (cases 3-5) and 52/53 (9,10), plus
            // everything outside 43..57.
            break;
        }
    }

    // =================================================================================
    //  HandleCrashNavInputReleased  @0x824CCD90  (cpp:446)
    //
    //  The two hold-to-act arms letting go. Both restore the medium zoom and put the
    //  cursor back into icon-selection mode; the zoom arm also plays the release sound
    //  and the inspect arm withdraws the inspected-icon publication on both channels.
    //  Note there is NO switch here -- the console compares the action id twice
    //  (`cmpwi 0x38` then `cmpwi 0x39`).
    // =================================================================================
    void CrashNavMapMain::HandleCrashNavInputReleased(const CgsModule::Event* lpEvent)
    {
        const s32 liAction =
            *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4);

        if (liAction == KI_ACTION_GUI_LTRIGGER)          // 56
        {
            mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_MEDIUM,
                                      KF_MAP_ZOOM_UNUSED_CUSTOM, false);

            GuiAudioTriggerEvent lAudio;
            lAudio.Construct(KI_AUDIO_ACTION_MAP_ZOOM_OUT, KAC_EMPTY_STRING,
                             KPC_SOUND_MAP_ZOOM, KAC_EMPTY_STRING);

            // The console memcpy's the 100-byte PAYLOAD out of the freshly-built event
            // and re-boxes it under wire id 457 (see GuiAudioTriggerPayload457's note).
            // The committed PC GuiAudioTriggerEvent carries its 12-byte GuiEvent<201>
            // header in front of that payload, so the copy starts at macComponent.
            GuiAudioTriggerPayload457 lPayload;
            std::memcpy(&lPayload, &lAudio.macComponent, sizeof(lPayload));

            CgsGui::GuiEventWrapper<GuiAudioTriggerPayload457, KI_CHANNEL_GUI_EVENT>
                lRecord(lPayload);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRecord),
                lRecord.GetChannel(), static_cast<s32>(sizeof(lRecord)));

            meCursorMode = E_CURSORMODE_SELECTING_ICONS;   // `li r11, 1 ; stw 0x5F50`
        }
        else if (liAction == KI_ACTION_GUI_RTRIGGER)     // 57
        {
            // `stw r30(=0), 0x609C(r31)` -- clear muInspectingEventID FIRST, then post
            // the same { 4, 558, 12, 0 } record on channel 41 and again on channel 42.
            muInspectingEventID = 0;

            GuiEventSetInspectedEventIcon lInspected;
            const u32 luNone = 0;
            std::memcpy(lInspected.maData, &luNone, sizeof(luNone));
            mpStateInterface->OutputViewState<GuiEventSetInspectedEventIcon>(lInspected);
            mpStateInterface->OutputInternalState<GuiEventSetInspectedEventIcon>(lInspected);

            mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_MEDIUM,
                                      KF_MAP_ZOOM_UNUSED_CUSTOM, false);

            meCursorMode = E_CURSORMODE_SELECTING_ICONS;
        }
    }
}
