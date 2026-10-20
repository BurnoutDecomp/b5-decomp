// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX_StuntGuiEvents.cpp
//
// ⭐⭐ [gateui] THE GAME-STATE -> GUI STUNT-COLLECTIBLE ARMS. Two BrnGame::BrnGameModule
// members whose DWARF home is GameSource/Game/GameBridgeGameStateToX.cpp:
//
//   MapStuntEnumsFromGameplayToGui   0x823AA4A8   (a real X360 symbol, whole)
//   TranslateGameActionsToGuiEvents  0x823E9CE0   (PARTIAL; see split-dispatch inventory)
//
// WHY A SIBLING TU RATHER THAN THE PARENT FILE. Historical: GameBridgeGameStateToX.cpp
// neither compiled nor linked when this slice landed. Both are fixed -- the parent TU
// compiles and is MOUNTED, its takedown translator having dropped the two invented
// accessors that were half of its link hole. What is still un-homed lives in the OTHER
// sibling, GameBridgeGameStateToX_Controller.cpp (see its banner for the four symbols),
// which is why that one is unmounted and this one is not. The functions HERE are called
// every sub-step by BrnGameModule::Update, so they must stay linkable. Split, exactly as
// ConvertTrainingTypeToStringId was split into GameBridgeGameStateToX_TrainingStringIds.cpp
// on 2026-08-16 -- MOVED, not copied, so folding these siblings back into the parent is a
// delete, not a duplicate-symbol hunt.
//
// The shared GUI-event push (PushGuiEvent) lives in GameBridgeGameStateToX.h so both TUs
// use one copy; its banner explains why the queue is written directly instead of through
// CgsGui::GuiModule::AddGuiEvent<T>.
// ============================================================================

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGameStateToX.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // original formatted rivalry assertions
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"               // BrnGui::GuiAutosaveRequestEvent (id 356)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                    // BrnGui::StuntType / GuiEventStunt*
#include "GameSource/Gui/Events/BrnGuiEventRankProgressResponse.h" // GuiEventRankProgressResponse (case 181)
#include "GameSource/Gui/Events/BrnGuiEventStatsResponse.h"        // GuiEventStatsResponse (case 180)
#include "GameSource/GameState/SharedIO/BrnGameActionData.h"        // GameStateModuleIO::GameStats (the case-180 record)
#include "GameSource/GameState/Progression/BrnProgressionManager.h" // ProgressionManager / Profile (case 180)
#include "SharedClasses/Trigger/BrnGenericRegion.h"                  // GenericRegion::Type (the drive-thru set query)
#include "GameSource/GameState/BrnGameActions.h"                   // the action payload homes
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"         // InviteOrJoinParams (action 92)
#include "GameSource/GameState/BrnGameStateModuleIO.h"             // OutputBuffer / GameActionQueue
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"             // InputBuffer::GetGuiEvents()
#include "GameShared/GameClasses/Gui/CgsGuiModule.h"               // streaming handshake publishers
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // CgsDev::Log::gpDebugPrint
#include "GameSource/Gui/Events/BrnGuiChallengeEvents.h"           // GUI 574 / 576 / 577 (free-burn challenge arms)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"           // CgsSystem::Time (the action-152 record)
#include "GameShared/GameClasses/System/PC/BrnNetHarnessPC.h"      // bounded [netui] witness lines
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                          // ModeManager::GetScoringSystem (case 279)
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"                // ScoringSystem::GetCrashScorer (case 279)
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"  // CrashModeScoring (case 279)
#include "GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.h"                  // HUDMessageLogic::ComboPerformedMessage (case 133)
#include <stdlib.h>                                               // getenv (the [UI-gate] diag guard)
#include <cstring>                                                 // memset/strncpy (the 537 ticker record)

namespace BrnGame
{
namespace
{
    // =========================================================================
    // ⭐⭐ [tut-ticker] the on-queue record of GUI event 537, TU-LOCAL (the exact precedent:
    // BrnCarSelectLivery_Components.cpp's GuiTickerCustomMessagePayload -- the canonical
    // BrnGui::GuiEventTickerCustomMessage home is in BrnGuiDemangledEventTypes.h, whose
    // opaque 12B-header shape does NOT match the wire: the X360
    // AddGuiEvent<GuiEventTickerCustomMessage> @0x823D1D08 posts `AddEvent(q, a2, 537, 2072)`
    // with a2 = the PAYLOAD BASE (case 148 hands it `addi r4, r1, var_2680`), i.e. the queued
    // 2072 bytes open with maiStringTypes, not with a GuiEvent header).
    // Layout recovered from GuiEventTickerCustomMessage::AddString @0x823A6940 (count at
    // +0x810 == a1+2064, types stride 4 at +0, strings stride 512 at +0x10; its three asserts
    // bake BrnGuiEventTypeDefs.h:390/391/392).
    // =========================================================================
    struct TickerCustomMessageWire537
    {
        s32  maiStringTypes[4];                 // +0x000
        char maacStrings[4][512];               // +0x010
        s8   mi8NumStrings;                     // +0x810
        u8   maFlags[4];                        // +0x811
        u8   maPad815[3];                       // +0x815

        s32 GetEventType() const { return 537; }

        // X360 0x823A6940, transcribed (the console's own bounds asserts, then
        // strncpy(base + 0x10 + count*512, str, 512); types[count] = type; ++count).
        void AddString(const char* lpString, s32 liType)
        {
            CGS_ASSERT(mi8NumStrings >= 0, "mi8NumStrings >= 0");                        // h:390
            CGS_ASSERT(mi8NumStrings < 4, "mi8NumStrings < KI_MAX_NUM_STRINGS");         // h:391
            CGS_ASSERT(lpString != 0, "lpString");                                       // h:392
            std::strncpy(maacStrings[mi8NumStrings], lpString, 512);
            maiStringTypes[mi8NumStrings] = liType;
            ++mi8NumStrings;
        }
    };
    static_assert(sizeof(TickerCustomMessageWire537) == 2072,
                  "X360 AddGuiEvent<GuiEventTickerCustomMessage> posts 2072 bytes (id 537)");

    // GUI event 356 is posted as one byte, id 356 size 1, through its canonical record.
    static_assert(sizeof(BrnGui::GuiAutosaveRequestEvent) == 1, "id 356 size 1");

    // ARTIST case 224 calls the one-byte AddGuiEvent<GuiEvent<152>> instantiation.
    // Keep this one-byte legacy tag local: the normal GUI 152 header is 12 bytes and
    // GuiHudMessage uses it, while this translator passes only its untouched stack byte.
    // No matching source consumer for this one-byte route is claimed.
    struct GuiEvent152UnwrittenByte
    {
        u8 mu8Unwritten;
        s32 GetEventType() const { return 152; }
    };
    static_assert(sizeof(GuiEvent152UnwrittenByte) == 1,
                  "ARTIST action 224 posts GUI 152 as one unwritten byte");

    // =========================================================================
    // [drive-thru wave 2026-08-29] The byte the BODY-SHOP drive-thru action (97) carries its
    // "the repair did something" flag in. The 144-byte shop payload is a serialised action
    // blob, not a C++ class: transform @+0x00, an identity block @+0x40, per-action scalars
    // from +0x80. For action 97 the producer (DriveThruManager::PostShopAction) writes the
    // player entity id at +0x80, the not-online flag at +0x84 and this byte at +0x85, and the
    // console's bridge arm reads exactly it back (`lbz r11, 0x85(r31)` @0x823EB5C8).
    // =========================================================================
    const s32 KI_SHOP_ACTION_EFFECTIVE_BYTE = 0x85;

    // =========================================================================
    // ⭐⭐⭐ [boost-ticker wave 2026-09-14] THE SIX BOOST-TICKER GUI WIRE RECORDS, TU-LOCAL. The
    // canonical BrnGui::GuiNearMissEvent / GuiDriftingEvent / GuiSpinningEvent / GuiInAirEvent /
    // GuiOncomingEvent / GuiTailgatingEvent / GuiTrafficCheckEvent live in
    // BrnGuiDemangledEventTypes.h (now included above). Each record below is field-for-field its
    // canonical twin -- no fork of meaning, only of scope (file-static in an anonymous
    // namespace, so there is no ODR fork either).
    //
    // WIRE PROOF for every id + size: the console's own AddGuiEvent<T> instantiation, whose
    // ONLY xref is TranslateGameActionsToGuiEvents @0x823E9CE0:
    //     GuiTrafficCheckEvent 0x823D3F88 -> AddEvent(q, e, 383, 4)
    //     GuiNearMissEvent     0x823D4040 -> AddEvent(q, e, 384, 8)
    //     GuiDriftingEvent     0x823D40F8 -> AddEvent(q, e, 385, 4)
    //     GuiSpinningEvent     0x823D41B0 -> AddEvent(q, e, 386, 4)
    //     GuiInAirEvent        0x823D4268 -> AddEvent(q, e, 387, 8)
    //     GuiOncomingEvent     0x823D4320 -> AddEvent(q, e, 388, 4)
    //     GuiTailgatingEvent   0x823D43D8 -> AddEvent(q, e, 389, 4)
    // and the far end is BrnGui::BoostMessageManager::RecvEvent @0x824204E8, whose cases
    // 383..389 read exactly these fields (already committed, already routed by
    // BrnRaceMainHudState_wS3.cpp's LABEL_120).
    // =========================================================================
    struct TrafficCheckEventWire383
    {
        s32 miCount;                      // +0x00
        s32 GetEventType() const { return 383; }
    };
    struct NearMissEventWire384
    {
        s32 miCount;                      // +0x00
        s32 meNearMissType;               // +0x04 (BrnWorld::ENearMissType; RecvEvent treats
                                          //        2 and 3 as the crash-escape flavours)
        s32 GetEventType() const { return 384; }
    };
    struct DriftingEventWire385
    {
        f32 mfDistance;                   // +0x00
        s32 GetEventType() const { return 385; }
    };
    struct SpinningEventWire386
    {
        f32 mfSpinAngle;                  // +0x00
        s32 GetEventType() const { return 386; }
    };
    struct InAirEventWire387
    {
        f32 mfCumulativeAirTime;          // +0x00
        f32 mfCurrentJumpAirTime;         // +0x04
        s32 GetEventType() const { return 387; }
    };
    struct OncomingEventWire388
    {
        f32 mfDistance;                   // +0x00
        s32 GetEventType() const { return 388; }
    };
    struct TailgatingEventWire389
    {
        f32 mfDistance;                   // +0x00
        s32 GetEventType() const { return 389; }
    };
    static_assert(sizeof(TrafficCheckEventWire383) == 4, "GUI 383 size 4");
    static_assert(sizeof(NearMissEventWire384)     == 8, "GUI 384 size 8");
    static_assert(sizeof(DriftingEventWire385)     == 4, "GUI 385 size 4");
    static_assert(sizeof(SpinningEventWire386)     == 4, "GUI 386 size 4");
    static_assert(sizeof(InAirEventWire387)        == 8, "GUI 387 size 8");
    static_assert(sizeof(OncomingEventWire388)     == 4, "GUI 388 size 4");
    static_assert(sizeof(TailgatingEventWire389)   == 4, "GUI 389 size 4");

    // =========================================================================
    // [crash-parity 2026-09-22] the on-queue record of GUI event 374
    // (BrnGui::GuiShutdownFinishedEvent), TU-LOCAL like the records above; the canonical home
    // is BrnGuiDemangledEventTypes.h.
    // Wire shape: AddGuiEvent<GuiShutdownFinishedEvent> @0x823D8A48 posts ONE byte
    // (`li r5, 0x176 ; li r6, 1` @0x823D8AE4..0x823D8AE8).
    // =========================================================================
    struct ShutdownFinishedEventWire374
    {
        u8 mu8Unwritten;                        // +0x00 (never written by the console's arm)
        s32 GetEventType() const { return 374; }
    };
    static_assert(sizeof(ShutdownFinishedEventWire374) == 1, "GUI 374 size 1");

    // [FX-FLOW 2026-09-24, G13-X5 remainder] the on-queue record of GUI event 373
    // (BrnGui::GuiShutdownEvent, DWARF BrnGuiEventTypeDefs.h:3618 {CgsID mVictimCarID}), TU-LOCAL
    // beside its 374 sibling. Wire shape: AddGuiEvent<GuiShutdownEvent>
    // @0x823D8990 posts the 8-byte id (`li r6, 8 ; li r5, 0x175` @0x823D8A2C..0x823D8A30).
    struct ShutdownEventWire373
    {
        CgsID mVictimCarID;                     // +0x00
        s32 GetEventType() const { return 373; }
    };
    static_assert(sizeof(ShutdownEventWire373) == 8, "GUI 373 size 8");

    // [FX-FLOW 2026-09-24, G11-D1 remainder] the on-queue record of GUI event 421
    // (BrnGui::GuiInEventNeckAndNeck, no members), TU-LOCAL
    // (its home is BrnGuiDemangledEventTypes.h). Wire shape:
    // AddGuiEvent<GuiInEventNeckAndNeck> @0x823D5688 posts ONE byte (`li r6, 1 ; li r5, 0x1A5`
    // @0x823D5724..0x823D5728); the translator's case 246 hands it a stack byte it never writes.
    struct NeckAndNeckEventWire421
    {
        u8 mu8Unwritten;                        // +0x00 (never written by the console's arm)
        s32 GetEventType() const { return 421; }
    };
    static_assert(sizeof(NeckAndNeckEventWire421) == 1, "GUI 421 size 1");

    // [DIAG] NOT IN THE X360 BINARY -- the race-mode HUD message rung (BRN_MODEMGR_DIAG, first 24
    // lines): which HUDMessageLogic action became which GUI event, with its car slot and value
    // (the split seconds for 245, the finish place for 247).
    void RaceHudBridgeDiag(s32 liAction, s32 liGuiEvent, s32 liCarIndex, f32 lfValue)
    {
        static const bool sbDiag      = (getenv("BRN_MODEMGR_DIAG") != 0);
        static s32        siLinesLeft = 24;
        if (!sbDiag || CgsDev::Log::gpDebugPrint == 0 || siLinesLeft <= 0)
        {
            return;
        }
        --siLinesLeft;
        *CgsDev::Log::gpDebugPrint << "[race-hud] action " << liAction << " -> gui " << liGuiEvent
                                   << " (car slot " << liCarIndex << ", value " << lfValue << ")\n";
    }

    // The free-burn challenge GUI records the 152..160 arms post, TU-local (their canonical
    // opaque homes are in BrnGuiDemangledEventTypes.h). Member
    // names are the original ones. Sizes are the console AddGuiEvent<T> sizes.
    struct OnlineTimeoutWire108
    {
        f32 mfTimeLeft;                         // +0x00
        s32 GetEventType() const { return 108; }
    };
    static_assert(sizeof(OnlineTimeoutWire108) == 4, "GUI 108 size 4");

    struct ShowFreeburnChallengeWire582
    {
        CgsID mChallengeID;                     // +0x00
        s32 GetEventType() const { return 582; }
    };
    static_assert(sizeof(ShowFreeburnChallengeWire582) == 8, "GUI 582 size 8");

    struct FburnChallengeEveryPlayerStatusWire581
    {
        BrnGameState::GameStateModuleIO::FburnChallengeEveryPlayerStatusData mStatus;   // +0x00
        s32 GetEventType() const { return 581; }
    };
    static_assert(sizeof(FburnChallengeEveryPlayerStatusWire581) == 2104, "GUI 581 size 2104");

    // GUI 104, 579 and 584 are tag-only GuiEvent<N> records: the console queues one
    // unwritten byte for each.
    void PushGuiTag(s32 liEventId, CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput)
    {
        const u8 lu8Tag = 0;
        lpGuiInput->GetGuiEvents()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lu8Tag), liEventId, 1);
    }
}

    // =========================================================================
    // ⭐ [gateui] BrnGameModule::MapStuntEnumsFromGameplayToGui  @ X360 0x823AA4A8
    //
    // The gameplay-side StuntElementType -> BrnGui::StuntType map. Asm-exact
    // (@0x823AA4B8..0x823AA57C): `cmplwi r27,1 / blt -> 0 / beq -> 1 / cmplwi r27,3 /
    // blt -> 2` and otherwise the streamed assert then `li r3,3`. An UNSIGNED compare, so a
    // negative gameplay value takes the assert arm, not the `< 1` one.
    // The assert text is built with the offending value streamed in
    // ("Invalid Stunt Enum : " << v << "\n") at GameBridgeGameStateToX.cpp:404.
    // It is a non-static member whose body never touches `this` -- reproduced as declared.
    // =========================================================================
    BrnGui::StuntType BrnGameModule::MapStuntEnumsFromGameplayToGui(u32 luGameplayStuntType) const
    {
        if (luGameplayStuntType < 1u)
        {
            return BrnGui::E_STUNTTYPE_JUMP;    // 0
        }
        if (luGameplayStuntType == 1u)
        {
            return BrnGui::E_STUNTTYPE_SMASH;   // 1
        }
        if (luGameplayStuntType < 3u)
        {
            return BrnGui::E_STUNTTYPE_STUNT;   // 2
        }

        // The console streams the offending value into the message before firing.
        CGS_ASSERT(false, "Invalid Stunt Enum");
        return BrnGui::E_STUNTTYPE_COUNT;       // 3 -- out of range on purpose (the analyzer
                                                //      bound-asserts it downstream)
    }

    // =========================================================================
    // ⭐⭐ [gateui] BrnGameModule::TranslateGameActionsToGuiEvents  @ X360 0x823E9CE0
    //
    // Drain the game-state output buffer's GameActionQueue (VariableEventQueue<13312,16>,
    // OutputBuffer +0x04, accessor X360 sub_823B96F0) and post the matching BrnGui event for
    // each recognised game action. Signature from the asm prologue @0x823E9D04..0x823E9D10
    // (r29 = this, r20 = lpGuiInput, r31 = lpGameStateOutput); the two null asserts are
    // GameBridgeGameStateToX.cpp:754/755. The queue walk is the console's own
    // GetFirstEvent / GetNextEvent pair (@0x823E9D88), and `r30 = 0x6EAA20` (7252512) is the
    // embedded CgsGui::GuiModule every arm pushes through -- see PushGuiEvent above for why
    // the push is written against the input buffer's queue instead.
    // Sole caller: BridgeGameStateToGui @0x823EE880 (call site @0x823EF22C).
    //
    // PARTIAL RECONSTRUCTION. ARTIST has 188 unique explicit top-level action ids.
    // This source snapshot represents 140/188; the report records each handled arm and the
    // 48 explicit ids still unrepresented. The disassembly's separate default-arm list remains
    // default behavior and is not conflated with the explicit-case inventory.
    // This count concerns action ids, independent of debug-menu registration counts. The queue
    // walk is active every frame in the current PC source, so omitted live actions remain gaps;
    // do not infer a no-op from a caption or missing direct xref.
    //
    // ⓘ ACTION IDS ARE THE X360 ONES, taken from the enum, never from literals:
    // BrnGameActions.h carries E_ACTION_ON_STUNT_ELEMENT_COMPLETE = 58 / _FOR_COUNTY = 59 /
    // _BY_TYPE = 60 (the PS3 DWARF values are 53/54/55 -- X360 == DWARF + 5 in this range).
    // Game EVENT ids are NOT shifted; only ACTION ids are.
    // =========================================================================
    void BrnGameModule::TranslateGameActionsToGuiEvents(
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput)
    {
        CGS_ASSERT(lpGuiInput != 0, "lpGuiInput");                   // :754
        CGS_ASSERT(lpGameStateOutput != 0, "lpGameStateOutput");     // :755
        if (lpGuiInput == 0 || lpGameStateOutput == 0)
        {
            return;
        }

        const BrnGameState::GameStateModuleIO::GameActionQueue* lpActionQueue =
            lpGameStateOutput->GetGameActionQueue();
        if (lpActionQueue == 0)
        {
            return;
        }

        // [DIAG] the `[UI-gate]` ladder's Gui-event rung. NOT IN THE X360 BINARY. Same logger,
        // same env guard (BRN_PROP_DIAG) and same first-N latch as the rest of the ladder.
        static const bool sbPropDiag      = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        static s32        siDiagLinesLeft = 8;

        // ⭐ [minimap blips, issue #9, 2026-09-07] THE PENDING SAT-NAV RECORD. The console keeps ONE
        // GuiEventUpdateSatNav (2320 bytes) on its frame across the whole drain, with its icon count
        // zeroed at entry; the case-45 arm APPENDS into it and the tail posts it ONCE, after the
        // loop, when the count is > 0. Static, not stack: the single-threaded build's oversized-
        // event-local convention (the world bridge's own per-frame 199 record does the same). Its
        // never-written tail bytes stay zero here where the console's frame local carried junk.
        static BrnGui::GuiEventUpdateSatNav lPendingSatNavEvent;
        s32 liPendingSatNavIcons = 0;

        const CgsModule::Event* lpAction     = 0;
        s32                     liActionSize = 0;
        s32                     liActionType = lpActionQueue->GetFirstEvent(&lpAction, &liActionSize);

        while (lpAction != 0)
        {
            switch (liActionType)
            {
            // ARTIST 0x823ED21C..0x823ED270: player-car change.
            case 1:
                CGS_ASSERT(*reinterpret_cast<const CgsID*>(lpAction) != 0,
                           "lChangeCarEvent.mCarId != kCGSID_NULL");
                lpGuiInput->GetGuiEvents()->AddEvent(lpAction, 415, sizeof(CgsID));
                break;
            // ARTIST 0x823ED278..0x823ED2CC: a car unlock is a separate
            // notification; it must not replace the car currently being driven.
            case 2:
                CGS_ASSERT(*reinterpret_cast<const CgsID*>(lpAction) != 0,
                           "lCarUnlockEvent.mCarId != kCGSID_NULL");
                lpGuiInput->GetGuiEvents()->AddEvent(lpAction, 76, sizeof(CgsID));
                break;
            // ARTIST 0x823ED2D4..0x823ED328: a respawn also publishes the
            // selected model from ResetPlayerCarAction+0x20.
            case BrnGameState::GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR:
            {
                const auto& lrReset = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::ResetPlayerCarAction*>(lpAction);
                CGS_ASSERT(lrReset.mCarModelId != 0,
                           "lChangeCarEvent.mCarId != kCGSID_NULL");
                lpGuiInput->GetGuiEvents()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lrReset.mCarModelId),
                    415, sizeof(CgsID));
                break;
            }
            // ARTIST823EBBD0..823EBC0C: request the GUI's fourth streaming
            // reply, then translate the loading-state byte without changing the wait.
            case BrnGameState::GameStateModuleIO::E_ACTION_WAIT_FOR_STREAMING:
            {
                CgsGui::GuiEvent<134> lEvent;
                CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_SET_LOADING_STATE:
            {
                if (*reinterpret_cast<const u8*>(lpAction) != 0)
                {
                    CgsGui::GuiEvent<136> lEvent;
                    CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInput);
                }
                else
                {
                    CgsGui::GuiEvent<137> lEvent;
                    CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInput);
                }
                break;
            }
            // ARTIST 0x823EBBA4..0x823EBBC8 copies the native action's two pointer words and
            // availability byte into the typed 12-byte X360 GUI 350 record. The named action and
            // event types widen their pointers together on PC; no second profile producer is added.
            case BrnGameState::GameStateModuleIO::E_ACTION_PROGRESSION_PROFILE_DATA:
            {
                const auto& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::ProgressionProfileDataAction*>(lpAction);
                BrnGui::GuiEventProgressionProfileData lEvent;
                lEvent.mpProfile = lrAction.mpProgressionProfile;
                lEvent.mpProgressionData = lrAction.mpProgressionData;
                lEvent.mbRoadRulesAvailable = lrAction.mbAreRoadRulesAvailable;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED838-0x823ED84C: release the GUI's model-change wait.
            case 66:
                lpGuiInput->GetGuiEvents()->AddEvent(lpAction, 565, sizeof(CgsID));
                break;

            // ARTIST 0x823EC5C8..0x823EC610, action 11: preserve the two setter calls and their
            // order. The 8-byte GUI record is {active-car index, network player id} (event 267).
            case BrnGameState::GameStateModuleIO::E_ACTION_REMOTE_PLAYER_DISCONNECTED:
            {
                CGS_ASSERT(lpAction != 0, "lpNetworkRemotePlayerDisconnectAction");
                const auto& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::RemotePlayerDisconnectedAction*>(lpAction);
                BrnGui::GuiNetworkRemotePlayerDisconnectEvent lEvent = {};
                lEvent.SetNetworkPlayerID(lrAction.mPlayerID);
                lEvent.SetActiveRaceCarIndex(lrAction.meActiveRaceCarIndex);
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EB074..0x823EB0E4, action 15 -> GUI 390 / 32 bytes. The final three
            // source bytes are unwritten stack tail in the console's local; the typed event's
            // padding stays zero here, while every defined field follows the original stores.
            case 15:
            {
                CGS_ASSERT(lpAction != 0, "lpCompletedStuntAction != NULL");
                const auto& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::CompletedStuntAction*>(lpAction);
                BrnGui::GuiCompletedStuntEvent lEvent = {};
                lEvent.muStuntActionComplete = lrAction.muStuntActionComplete;
                lEvent.mfCompletedBarrelRollAngle = lrAction.mfCompletedBarrelRollAngle;
                lEvent.mfCompletedAirSpinAngle = lrAction.mfCompletedAirSpinAngle;
                lEvent.mfCompletedHandbreakTurnAngle = lrAction.mfCompletedHandbreakTurnAngle;
                std::memcpy(&lEvent.mbSuccessfulLanding, &lrAction.mbSuccessfulLanding,
                            sizeof(lEvent.mbSuccessfulLanding));
                lEvent.mfCompletedDriftTime = lrAction.mfCompletedDriftTime;
                lEvent.mfCompletedDriftDistance = lrAction.mfCompletedDriftDistance;
                lEvent.miCompletedBarrelRolls = lrAction.miCompletedBarrelRolls;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EB104..0x823EB148, action 16 -> GUI 391 / 24 bytes. The action's
            // console-only tail (+0x18/+0x1C) is not part of the GUI record and is not forwarded.
            case 16:
            {
                CGS_ASSERT(lpAction != 0, "lpInProgressStuntAction != NULL");
                const auto& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::InProgressStuntAction*>(lpAction);
                BrnGui::GuiInProgressStuntEvent lEvent = {};
                lEvent.muStuntActionInProgress = lrAction.muStuntActionInProgress;
                lEvent.mfInProgressBarrelRollAngle = lrAction.mfInProgressBarrelRollAngle;
                lEvent.mfInProgressAirSpinAngle = lrAction.mfInProgressAirSpinAngle;
                lEvent.mfInProgressHandbreakTurnAngle = lrAction.mfInProgressHandbreakTurnAngle;
                lEvent.mfInProgressDriftTime = lrAction.mfInProgressDriftTime;
                lEvent.mfInProgressDriftDistance = lrAction.mfInProgressDriftDistance;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EAD6C..0x823EAD78 posts the canonical one-byte GuiEvent<91> tag.
            // Its payload is unwritten; preserve that representation without assigning a value.
            case 22:
            {
                CgsGui::GuiEvent<91> lEvent;
                CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EACB0..0x823EACB8 compresses BRNFBFSM and discards the result; it
            // posts no GUI record. Preserve that original leftover explicitly.
            case 25:
                (void)CgsIDCompress("BRNFBFSM");
                break;

            // ARTIST 0x823EACBC..0x823EAD1C, case 26 gates on words {2 or 16, not 10}, then
            // posts RunFsm (GUI 144 / 24).
            case 26:
            {
                const s32* lpActionWords = reinterpret_cast<const s32*>(lpAction);
                if ((lpActionWords[0] == 2 || lpActionWords[0] == 16) && lpActionWords[1] != 10)
                {
                    BrnGui::GuiEventRunFsm lEvent = {};
                    lEvent.mFsmId = CgsIDCompress("BRNEVENTFSM");
                    lEvent.mInitialStateId = 0;
                    lEvent.meFsmToRun = BrnGui::E_GUI_HUD_EVENT;
                    lEvent.meFlowToUse = BrnGui::E_GUIFLOW_HUD;
                    PushGuiEvent(lEvent, lpGuiInput);
                }
                break;
            }

            // ARTIST 0x823EB390..0x823EB3A0, action 32 reaches the shared label that writes
            // E_SPLASH_STATE_STOP (1), then posts GUI 269 / 4 bytes.
            case 32:
            {
                BrnGui::GuiEventNetworkSplashEvent lEvent = {};
                lEvent.meSplashState = BrnGui::GuiEventNetworkSplashEvent::E_SPLASH_STATE_STOP;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EAC60..0x823EACAC, case 41: byte +4 enables the post; byte +5 selects
            // the optional PAUSED state.
            case 41:
            {
                const u8* lpActionBytes = reinterpret_cast<const u8*>(lpAction);
                if (lpActionBytes[4] != 0)
                {
                    BrnGui::GuiEventRunFsm lEvent = {};
                    lEvent.mFsmId = CgsIDCompress("BRNFBFSM");
                    lEvent.mInitialStateId = 0;
                    if (lpActionBytes[5] == 1)
                        lEvent.mInitialStateId = CgsIDCompress("PAUSED");
                    lEvent.meFsmToRun = BrnGui::E_GUI_HUD_FREEBURN;
                    lEvent.meFlowToUse = BrnGui::E_GUIFLOW_HUD;
                    PushGuiEvent(lEvent, lpGuiInput);
                }
                break;
            }

            // ARTIST 0x823ED8E0..0x823ED920, action 49: the leading qword is an already-compressed
            // overlay id. Do not call the string overload or compress it a second time.
            case 49:
            {
                CGS_ASSERT(lpAction != 0, "lpPopupRequestAction");
                CgsID lOverlayId = 0;
                std::memcpy(&lOverlayId, lpAction, sizeof(lOverlayId));
                BrnGui::GuiOverlayRequest lRequest;
                lRequest.Construct(lOverlayId);
                PushGuiEvent(lRequest, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED1C8..0x823ED1E8: copy CrashComboAction's {entry type, value}
            // words to the raw 8-byte GUI 347 record. No downstream case-347 reader was found.
            case BrnGameState::GameStateModuleIO::E_ACTION_CRASH_COMBO:
            {
                const auto& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::CrashComboAction*>(lpAction);
                BrnGui::GuiCrashComboEvent lEvent = {};
                std::memcpy(lEvent.maData, &lrAction.meEntryType, sizeof(lrAction.meEntryType));
                std::memcpy(lEvent.maData + sizeof(lrAction.meEntryType), &lrAction.mfValue,
                            sizeof(lrAction.mfValue));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EC178..0x823EC1AC forwards the 24-byte StuntInfo image as GUI 429.
            case 132:
            {
                using StuntPerformedAction = BrnGameState::HUDMessageLogic::StuntPerformedMessage;
                static_assert(sizeof(StuntPerformedAction) == 24, "X360 action 132 payload size");
                const auto& lrAction = *reinterpret_cast<const StuntPerformedAction*>(lpAction);
                BrnGui::GuiHUDMessageStuntPerformed lEvent = {};
                lEvent.mStuntInfo = lrAction.mStuntInfo;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EC1B0..0x823EC1DC posts GUI 430 / 8 bytes, then passes the
            // original action base to the embedded developer-challenge owner. The producer is
            // HUDMessageLogic::GenerateStuntMessage: ARTIST posts its three WasComboRecentlyPerformed
            // out-slots as one 12-byte action (f32, s32, bool), matching ComboPerformedMessage.
            case 133:
            {
                using ComboPerformedAction = BrnGameState::HUDMessageLogic::ComboPerformedMessage;
                static_assert(sizeof(ComboPerformedAction) == 12, "X360 action 133 payload size");
                static_assert(offsetof(ComboPerformedAction, mfComboTime) == 0, "X360 action 133 float");
                static_assert(offsetof(ComboPerformedAction, miComboScore) == 4, "X360 action 133 score");
                static_assert(offsetof(ComboPerformedAction, mbValidCombo) == 8, "X360 action 133 validity");
                const auto& lrAction = *reinterpret_cast<const ComboPerformedAction*>(lpAction);
                BrnGui::GuiHUDMessageComboPerformed lEvent = {};
                lEvent.miScore = lrAction.miComboScore;
                lEvent.mbBestScore = lrAction.mbValidCombo;
                PushGuiEvent(lEvent, lpGuiInput);
                GetGameStateModule().GetDeveloperChallengeManager()->OnStuntRunComboPerformed(
                    &lrAction.mfComboTime);
                break;
            }

            // ARTIST 0x823EC1E4..0x823EC1F4 posts one byte as GUI 431. The source local is
            // unwritten; HudMessageAnalyzer's case 431 ignores the byte and triggers a fixed tag.
            case 134:
            {
                BrnGui::GuiHUDMessageStuntTimeUp lEvent = {};
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED6F8..0x823ED710 copies ShowtimeIntroAction::mbStart (+0x10) to
            // GUI 392 / 1 byte; copy the field representation rather than converting bool.
            case BrnGameState::GameStateModuleIO::E_ACTION_SHOWTIME_INTRO_START:
            {
                const auto& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::ShowtimeIntroAction*>(lpAction);
                BrnGui::GuiShowtimeTriggered lEvent = {};
                std::memcpy(lEvent.maData, &lrAction.mbStart, sizeof(lEvent.maData));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED758..0x823ED7B4 publishes GUI 404 / 8 bytes before selecting
            // the online or offline stunt scorer from current GameMode::mbIsOnline (+172).
            case BrnGameState::GameStateModuleIO::E_ACTION_POWER_PARK_RESULT:
            {
                const auto* const lpcAction = reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::PowerParkResultAction*>(lpAction);
                BrnGui::GuiPowerParkResult lEvent = {};
                lEvent.meOutcome = lpcAction->meOutcome;
                lEvent.miOverallRating = lpcAction->miOverallRating;
                PushGuiEvent(lEvent, lpGuiInput);

                BrnGameState::ModeManager* const lpModeManager = GetGameStateModule().GetModeManager();
                BrnGameState::GameMode* const lpCurrentGameMode = lpModeManager->GetCurrentGameMode();
                BrnGameState::ScoringSystem* const lpScoringSystem = lpModeManager->GetScoringSystem();
                if (lpCurrentGameMode != 0 && lpCurrentGameMode->IsOnline())
                    lpScoringSystem->GetOnlineStuntScorer()->DealWithPowerPark(lpcAction);
                else
                    lpScoringSystem->GetStuntScorer()->DealWithPowerPark(lpcAction);
                break;
            }

            // ARTIST 0x823EBFF4..0x823EC008 unconditionally fires the original assertion and
            // posts no GUI record. Preserve its original message, source path and line.
            case 185:
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(
                    "Part of offline custom events - no longer exist, and certainly not handled by GUI",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/GameBridgeGameStateToX.cpp",
                    2284);
                CgsDev::Assert::EndAssert();
                break;

            // ARTIST 0x823ED854..0x823ED86C: online car id -> GUI 566 (8 bytes).
            case BrnGameState::GameStateModuleIO::E_ACTION_CAR_SELECTION_CHANGED_ONLINE:
            {
                const auto* lpChanged = reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::CarSelectionChangedOnlineAction*>(lpAction);
                BrnGui::GuiCarSelectionChangedOnline lEvent = {};
                std::memcpy(lEvent.maData, &lpChanged->mCarId, sizeof(lpChanged->mCarId));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EA6E8..0x823EA6FC: start junkyard car select with word 1.
            case BrnGameState::GameStateModuleIO::E_ACTION_CAR_SELECT_START_GUI_ON_GAME_START:
            {
                BrnGui::GuiCarSelectStartEvent lEvent = {};
                const s32 liCarSelectFlow = 1;
                std::memcpy(lEvent.maData, &liCarSelectFlow, sizeof(liCarSelectFlow));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED818..0x823ED834: zero input posts the one-byte ready-to-exit signal.
            case 74:
                if (*reinterpret_cast<const u8*>(lpAction) == 0)
                {
                    BrnGui::GuiCarSelectReadyToExitEvent lEvent = {};
                    PushGuiEvent(lEvent, lpGuiInput);
                }
                break;

            // ARTIST 0x823ED870..0x823ED888: forward the action byte as GUI 84.
            case 78:
            {
                BrnGui::GuiCarSelectAbortEvent lEvent = {};
                lEvent.maData[0] = *reinterpret_cast<const u8*>(lpAction);
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EA71C..0x823EA72C: GUI 85 is a one-byte tag.
            case 80:
                PushGuiTag(85, lpGuiInput);
                break;

            // ⭐⭐ [boost-wave2 2026-09-14] 287 -> GUI 380, the SHORTCUT latch. ARTIST
            // 0x823ED554..0x823ED56C, the console's `case 287`:
            //     lbz  r11, 0(r31)                 ; the action's one byte
            //     stb  r11, var_35D8(r1)
            //     bl   AddGuiEvent<BrnGui::GuiPlayerInShortcutEvent>   -> AddEvent(q, rec, 380, 1)
            // A raw one-byte payload, no GuiEvent header -- same shape as case 81 below.
            //
            // BOTH ENDS WERE ALREADY LIVE AND COULD NOT HEAR EACH OTHER, the same defect the
            // boost-ticker wave found on its seven arms:
            //   PRODUCER  BrnGameState::StreetManager::UpdateUpcomingStreets @0x82350A88 posts
            //             action 287 on each edge of the player's shortcut membership (AISection
            //             flag bit 0) -- reconstructed and running at
            //             BrnGameStateStreetManager_wB_10.cpp:206-222.
            //   CONSUMER  BrnGui::GuiCache::RecvEvent case 380 (BrnGuiCache.cpp:1637) latches
            //             mbInShortcut, and GuiModule forwards 380 in its whitelist
            //             (BrnGuiModule.cpp:1972).
            // This arm was the only missing link, so the GUI's "player is in a shortcut" state
            // was stuck false for the whole game.
            case 287:
                lpGuiInput->GetGuiEvents()->AddEvent(lpAction, 380, 1);
                break;
            // [image-gallery response path] These records are produced by
            // GameStateImageManagerBase (289..293) and consumed by the image renderer /
            // ImageGalleryState. The output IDs and record layouts are the X360 translator's
            // typed AddGuiEvent instantiations; the source producer's raw stack records are
            // statically compared with the named action types in that producer TU.
            case BrnGameState::GameStateModuleIO::E_ACTION_IMAGE_GALLERY_RENDER_IMAGE:
            {
                const auto* lpRenderImageAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::ImageGalleryRenderImageAction*>(
                        lpAction);
                CGS_ASSERT(lpRenderImageAction != 0, "lpRenderImageAction");

                BrnGui::GuiEventNetworkPlayerImage lEvent;
                lEvent.mpTexture      = lpRenderImageAction->mpTexture;
                lEvent.miTextureIndex = lpRenderImageAction->miTextureIndex;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_IMAGE_GALLERY_IMAGE_INFO:
            {
                const auto* lpImageInfoAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::ImageGalleryImageInfoAction*>(
                        lpAction);
                CGS_ASSERT(lpImageInfoAction != 0, "lpImageInfoAction");

                BrnGui::GuiImageGalleryImageInfoEvent lEvent;
                lEvent.mCaptureDate = lpImageInfoAction->mCaptureDate;
                lEvent.mWorldRegion = lpImageInfoAction->mWorldRegion;
                lEvent.miNumCaptures = lpImageInfoAction->miNumCaptures;
                lEvent.miSlotIndex = lpImageInfoAction->miSlotIndex;
                lEvent.mPlayerName = lpImageInfoAction->mPlayerName;
                lEvent.mbLocked = lpImageInfoAction->mbLocked;
                lEvent.mbHasDeleted = lpImageInfoAction->mbHasDeleted;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_IMAGE_GALLERY_COUNT_ACTION:
            {
                const auto* lpCountAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::ImageGalleryCountAction*>(
                        lpAction);
                BrnGui::GuiImageGalleryCollectedCountEvent lEvent;
                lEvent.meImageGalleryImageType = lpCountAction->meImageType;
                lEvent.miCollectedCount = lpCountAction->miUsedCount;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_IMAGE_GALLERY_DATA_ACTION:
            {
                const auto* lpDataAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::ImageGalleryDataAction*>(
                        lpAction);
                BrnGui::GuiImageGalleryCollectedDataEvent lEvent;
                lEvent.meImageGalleryImageType = lpDataAction->meImageType;
                lEvent.mActiveBitArray = lpDataAction->mActiveBitArray;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_IMAGE_GALLERY_LOAD_COMPLETE:
            {
                // ARTIST reads no action byte here; the one-byte load marker only advances
                // ImageGalleryState's pending-load notification.
                CgsGui::GuiEvent<523> lEvent;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            // ARTIST 0x823EA760 / 0x823EBC50: colour and unlocked-livery replies.
            // These are raw GUI payloads, without a GuiEvent header.
            case 81:
                lpGuiInput->GetGuiEvents()->AddEvent(lpAction, 414, 8);
                break;
            // ARTIST 0x823EA730..0x823EA748: copy the time word as GUI 82 (four bytes).
            case 82:
            {
                BrnGui::GuiCarSelectOnlineTimeLeftEvent lEvent = {};
                std::memcpy(lEvent.maData, lpAction, sizeof(lEvent.maData));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EA74C..0x823EA75C: GUI 83 is a one-byte tag.
            case 83:
                PushGuiTag(83, lpGuiInput);
                break;

            case 183:
            {
                const auto& lrCars =
                    *reinterpret_cast<const Array<CgsID, 8>*>(lpAction);
                Array<CgsID, 8> lReply;
                lReply.Clear();
                for (s32 liCar = 0; liCar < lrCars.GetLength(); ++liCar)
                    lReply.Append(lrCars.GetItem(liCar));
                lpGuiInput->GetGuiEvents()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lReply), 413, sizeof(lReply));
                break;
            }

            // ARTIST 0x823EBCA4..0x823EBD40 rebuilds GUI 412 from the 128-car action:
            // append exactly the live IDs, copy both 128-bit sets, then copy the maximum.
            case BrnGameState::GameStateModuleIO::E_ACTION_CAR_SELECTION_RESPONSE:
            {
                using CarSelectionAction = BrnGameState::GameStateModuleIO::CarSelectionResponseAction;
                static_assert(sizeof(CarSelectionAction) == 1072, "X360 action 184 payload size");
                const auto& lrAction = *reinterpret_cast<const CarSelectionAction*>(lpAction);
                BrnGui::GuiCarSelectionEvent lEvent;
                lEvent.maCars.Clear();

                // ARTIST GetLength returns in r27 and compares with signed cmpwi against zero.
                const s32 liCarCount = static_cast<s32>(lrAction.maCars.GetLength());
                for (s32 liCar = 0; liCar < liCarCount; ++liCar)
                {
                    lEvent.maCars.Append(lrAction.maCars.GetItem(static_cast<u32>(liCar)));
                }

                lEvent.mHasBeenDrivenBitArray = lrAction.mHasBeenDrivenBitArray;
                lEvent.mWreckedArray = lrAction.mWreckedArray;
                lEvent.miMaxAvailableCars = lrAction.miMaxAvailableCars;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EBD98/0x823EBDA4 posts the one-byte Blue Team Escaping tag (GUI 446).
            // The local byte is unwritten on X360; default-initialize the existing raw event type.
            case 162:
            {
                BrnGui::GuiBlueTeamIsEscapingEvent lEvent;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EBDAC/0x823EBDB8 posts the one-byte Blue Team Behind You tag (GUI 447).
            case 163:
            {
                BrnGui::GuiBlueTeamIsBehindYouEvent lEvent;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST case 164 reads the typed LeaderReachedBoundaryAction record emitted by
            // HUDMessageLogic::GenerateOnlineLeaderMilestoneMessage: {metres f32 @+0,
            // active-car index @+4, miles bool @+8}. Preserve its filtered distance diagnostics
            // after the GUI event, including the console's metres-to-display-unit factors.
            case 164:
            {
                using LeaderBoundaryAction = BrnGameState::GameStateModuleIO::LeaderReachedBoundaryAction;
                static_assert(sizeof(LeaderBoundaryAction) == 12, "X360 action 164 payload size");
                const auto& lrAction = *reinterpret_cast<const LeaderBoundaryAction*>(lpAction);
                if (lrAction.mbIsBoundaryInMiles)
                {
                    BrnGui::GuiLeaderPassedMileBoundaryEvent lEvent = {};
                    lEvent.meActiveRaceCarIndex = lrAction.meRaceCarIndex;
                    lEvent.mfDistanceToFinishInMetres = lrAction.mfDistanceInMetres;
                    PushGuiEvent(lEvent, lpGuiInput);
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    {
                        *CgsDev::Log::gpDebugPrint << "****** LEADER PASSED "
                                                  << (lrAction.mfDistanceInMetres * 0.0006213712f)
                                                  << "mi boundary! ********!\n";
                    }
                }
                else
                {
                    BrnGui::GuiLeaderPassedKMBoundaryEvent lEvent = {};
                    lEvent.meActiveRaceCarIndex = lrAction.meRaceCarIndex;
                    lEvent.mfDistanceToFinishInMetres = lrAction.mfDistanceInMetres;
                    PushGuiEvent(lEvent, lpGuiInput);
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    {
                        *CgsDev::Log::gpDebugPrint << "****** LEADER PASSED "
                                                  << (lrAction.mfDistanceInMetres * 0.001f)
                                                  << "km boundary! ********!\n";
                    }
                }
                break;
            }

            // ARTIST 0x823EBE88..0x823EBEF4 posts the player-eliminated record/tag, then the
            // optional last-blue-team-member tag. Preserve its order and unwritten tag bytes.
            case BrnGameState::GameStateModuleIO::E_ACTION_PLAYER_ELIMINATED:
            {
                const auto& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::PlayerEliminatedAction*>(lpAction);
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "****** PLAYER ELIMINATED - TRANSLATING TO GUI EVENT! ********!\n";
                }
                if (lrAction.mbLocalPlayerEliminated)
                {
                    BrnGui::GuiLocalPlayerEliminatedEvent lEvent;
                    PushGuiEvent(lEvent, lpGuiInput);
                }
                else
                {
                    BrnGui::GuiPlayerEliminatedEvent lEvent = {};
                    std::memcpy(lEvent.maData, &lrAction.meActiveRaceCarIndex,
                                sizeof(lrAction.meActiveRaceCarIndex));
                    PushGuiEvent(lEvent, lpGuiInput);
                }
                if (lrAction.mbLastBlueTeamMember)
                {
                    BrnGui::GuiLastBlueTeamMemberEvent lEvent;
                    PushGuiEvent(lEvent, lpGuiInput);
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "****** LAST BLUE TEAM MEMBER - TRANSLATING TO GUI EVENT! ********!\n";
                    }
                }
                break;
            }

            // ARTIST 0x823EBEF8..0x823EBF2C preserves the original assert and forwards the
            // aggressor's active-car index as GUI 453 / four bytes.
            case BrnGameState::GameStateModuleIO::E_ACTION_TRAITOROUS_TAKEDOWN:
            {
                if (lpAction == 0)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "lpTraitorousTDAction",
                        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/GameBridgeGameStateToX.cpp",
                        2219);
                    CgsDev::Assert::EndAssert();
                }
                const auto& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::TraitorousTakedownAction*>(lpAction);
                BrnGui::GuiTraitorousTakedownEvent lEvent = {};
                lEvent.meAggActiveRaceCarIndex = lrAction.meAggrActiveRaceCarIndex;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED62C..0x823ED670 posts GUI 541 only when action flag 0x04 is set.
            // It copies the 56-byte score block and maps flag 0x08 to the HUD-message byte.
            case BrnGameState::GameStateModuleIO::E_ACTION_BURNOUT_SKILLZ:
            {
                if (lpAction == 0)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "lpBurnoutSkillzAction",
                        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/GameBridgeGameStateToX.cpp",
                        3503);
                    CgsDev::Assert::EndAssert();
                }
                using BurnoutSkillzAction = BrnGameState::GameStateModuleIO::BurnoutSkillzAction;
                static_assert(sizeof(BurnoutSkillzAction) == 68 &&
                              offsetof(BurnoutSkillzAction, meActiveRaceCarIndex) == 0x40,
                              "X360 action 168 input layout");
                const auto& lrAction = *reinterpret_cast<const BurnoutSkillzAction*>(lpAction);
                if ((lrAction.miActionFlags & BurnoutSkillzAction::KI_ACTION_TYPE_UPDATED_VALUE) !=
                    BurnoutSkillzAction::KI_ACTION_TYPE_UPDATED_VALUE)
                {
                    break;
                }

                BrnGui::GuiNewBurnoutSkillzEvent lEvent;
                lEvent.mNetworkPlayerID = lrAction.mNetworkPlayerID;
                lEvent.meActiveRaceCarIndex = lrAction.meActiveRaceCarIndex;
                lEvent.mSkillzData = lrAction.mBurnoutSkillzData;
                lEvent.mbUpdateHUDMessage =
                    (lrAction.miActionFlags & BurnoutSkillzAction::KI_ACTION_TYPE_SHOW_HUD_MESSAGE) != 0;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED678..0x823ED6F0 posts GUI 542 / 24 bytes. The action's producer
            // type is not present in current source; this view names only the fields and offsets
            // consumed by the original arm and leaves its input-domain owner unresolved.
            case 169:
            {
                if (lpAction == 0)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "lpRoadRulesSkillzAction",
                        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/GameBridgeGameStateToX.cpp",
                        3526);
                    CgsDev::Assert::EndAssert();
                }
                struct NewBurnoutHudMessageActionView
                {
                    CgsID mRoadID;  // +0x00
                    u32 muActionValue;  // +0x08; ARTIST maps zero / one / other to message types 1/2/3
                    BrnGameState::BurnoutSkillzData::EBurnoutSkillType meSkill;  // +0x0C
                    EActiveRaceCarIndex meNewOwner;  // +0x10
                };
                static_assert(offsetof(NewBurnoutHudMessageActionView, muActionValue) == 0x08 &&
                              offsetof(NewBurnoutHudMessageActionView, meSkill) == 0x0C &&
                              offsetof(NewBurnoutHudMessageActionView, meNewOwner) == 0x10,
                              "ARTIST action 169 consumed input offsets");
                const auto& lrAction =
                    *reinterpret_cast<const NewBurnoutHudMessageActionView*>(lpAction);
                BrnGui::GuiNewBurnoutHudMessageEvent lEvent = {};
                lEvent.mRoadID = lrAction.mRoadID;
                if (lrAction.muActionValue == 0)
                    lEvent.meMessageType = BrnGui::GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_X_BEAT_YOUR;
                else if (lrAction.muActionValue == 1)
                    lEvent.meMessageType = BrnGui::GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_X_GOT;
                else
                    lEvent.meMessageType = BrnGui::GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_YOU_GOT;
                lEvent.meSkill = lrAction.meSkill;
                lEvent.meNewOwner = lrAction.meNewOwner;
                lEvent.mePreviousOwner = static_cast<EActiveRaceCarIndex>(-1);
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED184..0x823ED194: payback lost -> one unwritten GUI 178 tag.
            case 211:
            {
                CgsGui::GuiEvent<178> lEvent;
                CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED198..0x823ED1A8: payback victim left -> one unwritten GUI 180 tag.
            case 212:
            {
                CgsGui::GuiEvent<180> lEvent;
                CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ECECC..0x823ED01C: Payback mugshot control. Response 0 is an
            // intentional no-post arm; later capture/show steps copy the road id, response,
            // image type, selected car index and local-aggressor byte into GUI 325.
            case BrnGameState::GameStateModuleIO::E_ACTION_PAYBACK_MUGSHOT:
            {
                const auto* lpPaybackMugshotAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::PaybackMugshotAction*>(
                        lpAction);
                if (lpPaybackMugshotAction->meMugshotResponse ==
                    BrnGameState::GameStateModuleIO::E_MUGSHOT_RESPONSE_PREPARE_CAPTURE)
                {
                    break;
                }

                const bool lbLocalAggressor =
                    lpPaybackMugshotAction->mbIsTakedownAggressorLocalPlayer;
                const s32 liAggressorCarIndex =
                    lbLocalAggressor ? lpPaybackMugshotAction->meTakedownVictimIndex
                                     : lpPaybackMugshotAction->meTakedownAggressorIndex;

                BrnGui::GuiMugshotControlEvent lEvent;
                u8* lpEventBytes = reinterpret_cast<u8*>(&lEvent);
                std::memcpy(lpEventBytes, &lpPaybackMugshotAction->mRoadRuleBeatenRoadID, sizeof(CgsID));
                std::memcpy(lpEventBytes + 8, &lpPaybackMugshotAction->meMugshotResponse,
                            sizeof(lpPaybackMugshotAction->meMugshotResponse));
                std::memcpy(lpEventBytes + 12, &lpPaybackMugshotAction->meImageType,
                            sizeof(lpPaybackMugshotAction->meImageType));
                std::memcpy(lpEventBytes + 16, &liAggressorCarIndex, sizeof(liAggressorCarIndex));
                lpEventBytes[20] = static_cast<u8>(lbLocalAggressor);

                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "MUGSHOT EVENT : BRIDGE GAME STATE TO X (1)\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint
                        << "IMAGE TYPE : " << static_cast<s32>(lpPaybackMugshotAction->meImageType) << "\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "LOCAL AGG  : " << lbLocalAggressor << "\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint
                        << "AGG ARCI   : " << liAggressorCarIndex << "\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint
                        << "RESPONSE   : "
                        << static_cast<s32>(lpPaybackMugshotAction->meMugshotResponse) << "\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint
                        << "ROAD?      : "
                        << static_cast<u64>(lpPaybackMugshotAction->mRoadRuleBeatenRoadID) << "\n";
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED020..0x823ED180: abort an in-flight capture. The console's r19
            // zero register writes the first qword; response 4 and the action fields follow.
            case BrnGameState::GameStateModuleIO::E_ACTION_ABORT_MUGSHOT_CAPTURE:
            {
                const auto* lpAbortCaptureAction =
                    reinterpret_cast<const
                        BrnGameState::GameStateModuleIO::AbortMugshotCaptureAction*>(lpAction);
                CGS_ASSERT(lpAbortCaptureAction != 0, "lpAbortCaptureAction");

                const bool lbLocalAggressor =
                    lpAbortCaptureAction->mbIsTakedownAggressorLocalPlayer;
                const s32 liAggressorCarIndex =
                    lbLocalAggressor ? lpAbortCaptureAction->meTakedownVictimIndex
                                     : lpAbortCaptureAction->meTakedownAggressorIndex;
                const s32 liResponse = static_cast<s32>(
                    BrnGameState::GameStateModuleIO::E_MUGSHOT_RESPONSE_STOP_MY_MUGSHOT);

                BrnGui::GuiMugshotControlEvent lEvent;
                u8* lpEventBytes = reinterpret_cast<u8*>(&lEvent);
                std::memset(lpEventBytes, 0, sizeof(CgsID));
                std::memcpy(lpEventBytes + 8, &liResponse, sizeof(liResponse));
                std::memcpy(lpEventBytes + 12, &lpAbortCaptureAction->meImageType,
                            sizeof(lpAbortCaptureAction->meImageType));
                std::memcpy(lpEventBytes + 16, &liAggressorCarIndex, sizeof(liAggressorCarIndex));
                lpEventBytes[20] = static_cast<u8>(lbLocalAggressor);

                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "MUGSHOT EVENT : BRIDGE GAME STATE TO X (2)\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint
                        << "IMAGE TYPE : " << static_cast<s32>(lpAbortCaptureAction->meImageType) << "\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "LOCAL AGG  : " << lbLocalAggressor << "\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint
                        << "AGG ARCI   : " << liAggressorCarIndex << "\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "RESPONSE   : " << liResponse << "\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "ROAD?      : " << static_cast<u64>(0) << "\n";
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EB598..0x823EB5AC forwards the activated dirty-trick type as GUI 182 / 4 bytes.
            case BrnGameState::GameStateModuleIO::E_ACTION_PAYBACK_ACTIVATED:
            {
                using PaybackAction = BrnGameState::GameStateModuleIO::PaybackActivatedAction;
                static_assert(sizeof(PaybackAction) == 12, "X360 action 215 payload size");
                const auto& lrAction = *reinterpret_cast<const PaybackAction*>(lpAction);
                BrnGui::GuiPaybackReceivedEvent lEvent = {};
                std::memcpy(lEvent.maData, &lrAction.mePaybackType, sizeof(lrAction.mePaybackType));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EB5B4..0x823EB5C0: payback over -> one unwritten GUI 183 tag.
            case 216:
            {
                CgsGui::GuiEvent<183> lEvent;
                CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED1AC..0x823ED1C4: online player added -> lobby-join chatter.
            case BrnGameState::GameStateModuleIO::E_ACTION_ONLINE_PLAYER_ADDED:
            {
                const auto& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::OnlinePlayerAddedAction*>(lpAction);
                BrnGui::GuiEventNetworkPlayerJoinedLobby lEvent;
                lEvent.mNetworkPlayerID = lrAction.mAddedPlayerNetworkID;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EB7C8..0x823EB7D8: notify the crash HUD that skipping is available.
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_PLAYER_CAN_SKIP_CRASH:
            {
                // The console does not write this one-byte payload; the receiver ignores it.
                BrnGui::GuiEventCanSkipCrash lEvent;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EA87C..0x823EA890 and 0x823EA894..0x823EA8A8: pause-state edges.
            case 86:
            case 87:
            {
                BrnGui::GuiGamePausedEvent lEvent = {};
                lEvent.maData[4] = (liActionType == 86) ? 1u : 0u;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EA8AC..0x823EA8C0: literal true -> GUI show-HUD event 148.
            case 89:
            {
                BrnGui::GuiEventShowHideHud lEvent = {};
                lEvent.maData[0] = 1;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EBF54..0x823EBF70: construct the named offline-string overlay.
            case 90:
            {
                BrnGui::GuiOverlayRequest lRequest;
                lRequest.Construct("GMStrOffline");
                PushGuiEvent(lRequest, lpGuiInput);
                break;
            }

            // ARTIST 0x823EBF34..0x823EBF50: construct the invite-accept overlay.
            case 91:
            {
                BrnGui::GuiOverlayRequest lRequest;
                lRequest.Construct("GMInvAccept");
                PushGuiEvent(lRequest, lpGuiInput);
                break;
            }

            // ARTIST 0x823EBF74..0x823EBF8C, case 92 reads InviteOrJoinParams+0x98 and posts the
            // one-byte GUI 128 event.
            // Its real producer copies that same 0x9C-byte record from mInviteOrJoinParams.
            case 92:
            {
                const auto* lpParams = reinterpret_cast<const
                    BrnNetwork::BrnNetworkModuleIO::InviteOrJoinParams*>(lpAction);
                BrnGui::GuiEventPrepareForInvite lEvent = {};
                std::memcpy(lEvent.maData, &lpParams->mbIsLocalUserChangeNeeded,
                            sizeof(lEvent.maData));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EBF90..0x823EBFA0: update-prepare-for-invite sends tag 129.
            case BrnGameState::GameStateModuleIO::E_ACTION_UPDATE_PREPARE_FOR_INVITE:
                PushGuiTag(129, lpGuiInput);
                break;

            // ARTIST 0x823EBFA4..0x823EBFB4: perform-invite sends tag 131; action bytes are unread.
            case BrnGameState::GameStateModuleIO::E_ACTION_PERFORM_INVITE:
                PushGuiTag(131, lpGuiInput);
                break;

            // ARTIST 0x823EBFB8..0x823EBFF0: assert the record, then relay its success byte.
            case BrnGameState::GameStateModuleIO::E_ACTION_INVITE_COMPLETE:
            {
                CGS_ASSERT(lpAction != 0, "lpInviteCompleteAction");
                BrnGui::GuiEventInviteComplete lEvent = {};
                std::memcpy(lEvent.maData, lpAction, sizeof(lEvent.maData));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 45  E_ACTION_SET_UP_ALL_DRIVE_THRUS (1112 bytes) -------------------------
            // ⭐⭐ [minimap blips, issue #9] THE DRIVE-THRU ICON TABLE HOP. While the pending count
            // is below 48, one SatNavIconInfo per DriveThruInfo:
            //   * the GenericRegion::Type (info +0x10) becomes the icon-type byte (+0x28):
            //     0 JUNK_YARD -> 7 JUNKYARD, 1 GAS_STATION -> 10, 2 BODY_SHOP -> 9,
            //     3 PAINT_SHOP -> 11, 4 CAR_PARK -> 8, 16 TIRE_SHOP -> 12, anything else -> 4
            //     LANDMARK after a message-stream print of the type;
            //   * the position lane (+0x00) is {x, 0, z, 0} from the info's two floats;
            //   * the CgsID (+0x10) is copied whole; rotation (+0x18) and speed (+0x1C) are 0.0;
            //     the design-index (+0x22) and hidden-drive-thru (+0x23) bytes are 0.
            // Unlike the neighbouring arms this one posts NOTHING here -- see the tail.
            case BrnGameState::GameStateModuleIO::E_ACTION_SET_UP_ALL_DRIVE_THRUS:
            {
                typedef BrnGui::GuiEventUpdateSatNav::SatNavIconInfo SatNavIconInfo;
                typedef BrnGameState::GameStateModuleIO::SetUpAllDriveThrusAction SetUpAction;

                const SetUpAction* lpSetUp = reinterpret_cast<const SetUpAction*>(lpAction);
                const s32 liNumDriveThrus  = static_cast<s32>(lpSetUp->maDriveThrus.GetLength());

                for (s32 liDriveThru = 0; liDriveThru < liNumDriveThrus; ++liDriveThru)
                {
                    if (liPendingSatNavIcons >= BrnGui::GuiEventUpdateSatNav::KI_MAX_SAT_NAV_ICONS)
                    {
                        break;   // the pending record is full
                    }

                    const SetUpAction::DriveThruInfo& lrInfo =
                        lpSetUp->maDriveThrus.GetItem(static_cast<u32>(liDriveThru));
                    SatNavIconInfo& lrIcon = lPendingSatNavEvent.maIconInfo[liPendingSatNavIcons];

                    SatNavIconInfo::SatNavIconType leIconType;
                    switch (lrInfo.meType)
                    {
                    case BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD:   // 0
                        leIconType = SatNavIconInfo::E_SATNAVICON_JUNKYARD;      // 7
                        break;
                    case BrnTrigger::GenericRegion::E_TYPE_GAS_STATION: // 1
                        leIconType = SatNavIconInfo::E_SATNAVICON_GAS_STATION;   // 10
                        break;
                    case BrnTrigger::GenericRegion::E_TYPE_BODY_SHOP:   // 2
                        leIconType = SatNavIconInfo::E_SATNAVICON_BODYSHOP;      // 9
                        break;
                    case BrnTrigger::GenericRegion::E_TYPE_PAINT_SHOP:  // 3
                        leIconType = SatNavIconInfo::E_SATNAVICON_PAINT_SHOP;    // 11
                        break;
                    case BrnTrigger::GenericRegion::E_TYPE_CAR_PARK:    // 4
                        leIconType = SatNavIconInfo::E_SATNAVICON_CAR_PARK;      // 8
                        break;
                    case BrnTrigger::GenericRegion::E_TYPE_TIRE_SHOP:   // 16
                        leIconType = SatNavIconInfo::E_SATNAVICON_TIRE_SHOP;     // 12
                        break;
                    default:
                        // The console prints the offending type on the message stream first
                        // (gxMessageFilterFlags & 1; the prefix string is not recovered).
                        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
                        {
                            *CgsDev::Log::gpDebugPrint
                                << "[drivethru] BRIDGE: unexpected drive-thru region type "
                                << static_cast<s32>(lrInfo.meType) << "\n";
                        }
                        leIconType = SatNavIconInfo::E_SATNAVICON_LANDMARK;      // 4
                        break;
                    }

                    Vector4 lv4Position;
                    lv4Position.x = lrInfo.mfXCoord;
                    lv4Position.y = 0.0f;
                    lv4Position.z = lrInfo.mfZCoord;
                    lv4Position.w = 0.0f;
                    lrIcon.SetPositionLane(lv4Position);
                    lrIcon.SetCgsId(lrInfo.mDriveThruId);
                    lrIcon.SetRotation(0.0f);
                    lrIcon.SetSpeedMph(0.0f);
                    lrIcon.SetDesignIndex(0);
                    lrIcon.SetHiddenDriveThru(false);
                    lrIcon.SetIconType(leIconType);

                    ++liPendingSatNavIcons;
                }
                break;
            }

            // ---- 58  E_ACTION_ON_STUNT_ELEMENT_COMPLETE (24 bytes) ------------------------
            // ⭐⭐ THE BILLBOARD / SMASH-GATE HUD POPUP. @0x823EB870..0x823EB96C.
            //   lwz r11, 0x14(r31)   -- meCurrentGameMode picks the presentation
            //   cmplwi r11, 0x11 ; bgt default            (UNSIGNED: E_MODE_NONE == -1 -> default)
            //   jump table jpt_823EB894: cases 0-8,10-14,16,17 -> the BOOST-BAR arm,
            //                            cases 9,15 (+ anything > 17) -> the plain arm
            //   both arms: { miCurrentCount@+0x00 <- action+0x0C,
            //                miTotalCount  @+0x04 <- action+0x10,
            //                meStuntType   @+0x08 <- MapStuntEnumsFromGameplayToGui(action+0x08) }
            //   both arms then post a 1-byte GuiAutosaveRequestEvent (356) whose byte is 0.
            // The two events are identically shaped but distinct ids with distinct consumers:
            // 217 dispatches to HudMessageAnalyzer::HandleStuntInfo ("Billboards Smashed 12/45"),
            // 218 goes to the HUD boost-bar component.
            case BrnGameState::GameStateModuleIO::E_ACTION_ON_STUNT_ELEMENT_COMPLETE:
            {
                const BrnGameState::GameStateModuleIO::OnStuntElementCompleteAction* lpStunt =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::OnStuntElementCompleteAction*>(lpAction);

                const BrnGui::StuntType leGuiStuntType =
                    MapStuntEnumsFromGameplayToGui(
                        static_cast<u32>(lpStunt->meStuntElementType));

                // The autosave request is built ONCE at the head of the case with a zero byte
                // and posted from both presentation arms, after the stunt-info event.
                BrnGui::GuiAutosaveRequestEvent lAutosaveRequest;
                lAutosaveRequest.maData[0] = 0;

                bool lbBoostBarPresentation;
                switch (lpStunt->meCurrentGameMode)
                {
                // jpt_823EB894 cases 0-8, 10-14, 16, 17.
                case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE:
                case BrnGameState::GameStateModuleIO::E_MODE_FACE_OFF:
                case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME:
                case BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE:
                case BrnGameState::GameStateModuleIO::E_MODE_PURSUIT:
                case BrnGameState::GameStateModuleIO::E_MODE_BURNING_ROUTE:
                case BrnGameState::GameStateModuleIO::E_MODE_ELIMINATOR:
                case BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK:
                case BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN:
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_RACE:
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE:
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN:
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME:
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_END:
                    lbBoostBarPresentation = true;
                    break;
                // jpt_823EB894 default: cases 9 (traffic attack) and 15 (free-burn lobby), plus
                // everything the `cmplwi 0x11 / bgt` pre-test rejected.
                default:
                    lbBoostBarPresentation = false;
                    break;
                }

                s32 liPostedGuiEventId;
                if (lbBoostBarPresentation)
                {
                    BrnGui::GuiEventBoostBarStuntInfo lEvent;           // id 218, 12 bytes
                    lEvent.miCurrentCount = lpStunt->miCurrentCount;
                    lEvent.miTotalCount   = lpStunt->miTotalCount;
                    lEvent.meStuntType    = leGuiStuntType;
                    liPostedGuiEventId    = lEvent.GetEventType();
                    PushGuiEvent(lEvent, lpGuiInput);
                    PushGuiEvent(lAutosaveRequest, lpGuiInput);
                }
                else
                {
                    BrnGui::GuiEventStuntInfo lEvent;                   // id 217, 12 bytes
                    lEvent.miCurrentCount = lpStunt->miCurrentCount;
                    lEvent.miTotalCount   = lpStunt->miTotalCount;
                    lEvent.meStuntType    = leGuiStuntType;
                    liPostedGuiEventId    = lEvent.GetEventType();
                    PushGuiEvent(lEvent, lpGuiInput);
                    PushGuiEvent(lAutosaveRequest, lpGuiInput);
                }

                // [FLAG PC witness] NOT IN THE CONSOLE. Opt-in (BRN_DISCO_DIAG), first 16 lines.
                static const bool sbDiscoDiag      = (getenv("BRN_DISCO_DIAG") != 0);
                static s32        siDiscoLinesLeft = 16;
                if (sbDiscoDiag && siDiscoLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
                {
                    --siDiscoLinesLeft;
                    *CgsDev::Log::gpDebugPrint
                        << "[disco] action 58 -> gui " << liPostedGuiEventId
                        << " + gui 356 autosave request (type " << static_cast<s32>(leGuiStuntType)
                        << " count " << lpStunt->miCurrentCount << "/" << lpStunt->miTotalCount << ")\n";
                }

                if ( sbPropDiag && siDiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
                {
                    --siDiagLinesLeft;
                    *CgsDev::Log::gpDebugPrint
                        << "[UI-gate] gui-event id=" << liPostedGuiEventId
                        << " type=" << static_cast<s32>(leGuiStuntType)
                        << " count=" << lpStunt->miCurrentCount
                        << "/" << lpStunt->miTotalCount << "\n";
                }
                break;
            }

            // =====================================================================================
            // ⭐⭐⭐ [boost-ticker wave 2026-09-14] THE SEVEN BOOST-TICKER ARMS -- X360
            // TranslateGameActionsToGuiEvents @0x823E9CE0 cases 108 and 171..176.
            //
            // These are the second missing link of the boost hint strip. The first
            // (world event -> game action) is the boost-ticker arms of
            // GameStateModule::ProcessGameEvents; the far end (GUI event -> the hint strip) has been
            // committed and routed for weeks. Every arm below is the console's own body,
            // which in six of the seven cases is literally "copy the words, post the event":
            //
            //   console arm                                    ->  GUI event (id, size)
            //   case 108 @0x823EB..  v329 = *action                383 GuiTrafficCheckEvent (4)
            //   case 171 @0x823ED..  v463[0]=*a; v463[1]=a[1]      384 GuiNearMissEvent     (8)
            //   case 172             v377 = *a                     385 GuiDriftingEvent     (4)
            //   case 173             v423 = *a                     386 GuiSpinningEvent     (4)
            //   case 174             assert(a); v451[0..1]=a[0..1] 387 GuiInAirEvent        (8)
            //   case 175             v409 = *a                     388 GuiOncomingEvent     (4)
            //   case 176             v381 = *a                     389 GuiTailgatingEvent   (4)
            //
            // ⚠️ NOTE THE WIDTH ASYMMETRY ON 176 AND IT IS THE CONSOLE'S: the TailgatingAction
            // is EIGHT bytes {mfDistance, meTailgatedCarIndex} but the console's arm copies
            // ONE word (`v381 = *v7`) into a FOUR-byte GuiTailgatingEvent -- the tailgated car
            // index never reaches the GUI. Do not "complete" it.
            // =====================================================================================

            // ---- 108  E_ACTION_ON_TRAFFIC_CHECKING_CHAIN (4 bytes) -> GUI 383 ---------------
            case BrnGameState::GameStateModuleIO::E_ACTION_ON_TRAFFIC_CHECKING_CHAIN:
            {
                const BrnGameState::GameStateModuleIO::TrafficCheckingChainAction* lpChain =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::TrafficCheckingChainAction*>(lpAction);
                TrafficCheckEventWire383 lEvent;
                lEvent.miCount = lpChain->miChainSize;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 171  E_ACTION_NEAR_MISS (8 bytes) -> GUI 384 -------------------------------
            case BrnGameState::GameStateModuleIO::E_ACTION_NEAR_MISS:
            {
                const BrnGameState::GameStateModuleIO::NearMissAction* lpNearMiss =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::NearMissAction*>(lpAction);
                NearMissEventWire384 lEvent;
                lEvent.miCount        = lpNearMiss->miCount;
                lEvent.meNearMissType = lpNearMiss->meNearMissType;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 172  E_ACTION_DRIFTING (4 bytes) -> GUI 385 --------------------------------
            case BrnGameState::GameStateModuleIO::E_ACTION_DRIFTING:
            {
                const BrnGameState::GameStateModuleIO::DriftingAction* lpDrift =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::DriftingAction*>(lpAction);
                DriftingEventWire385 lEvent;
                lEvent.mfDistance = lpDrift->mfDistance;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 173  E_ACTION_SPINNING (4 bytes) -> GUI 386 --------------------------------
            case BrnGameState::GameStateModuleIO::E_ACTION_SPINNING:
            {
                const BrnGameState::GameStateModuleIO::SpinningAction* lpSpin =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::SpinningAction*>(lpAction);
                SpinningEventWire386 lEvent;
                lEvent.mfSpinAngle = lpSpin->mfSpinAngle;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 174  E_ACTION_IN_AIR (8 bytes) -> GUI 387 ----------------------------------
            case BrnGameState::GameStateModuleIO::E_ACTION_IN_AIR:
            {
                CGS_ASSERT(lpAction != 0, "lpInAirAction");   // GameBridgeGameStateToX.cpp:1983
                const BrnGameState::GameStateModuleIO::InAirAction* lpInAir =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::InAirAction*>(lpAction);
                InAirEventWire387 lEvent;
                lEvent.mfCumulativeAirTime  = lpInAir->mfCumulativeAirTime;
                lEvent.mfCurrentJumpAirTime = lpInAir->mfCurrentJumpAirTime;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 175  E_ACTION_ONCOMING (4 bytes) -> GUI 388 --------------------------------
            // THE "ONCOMING 800m" COUNTER: BoostMessageManager::UpdateOncoming composes
            // "<ONCOMING> GENERAL_SEPARATOR <distance>m" once the latched distance passes 50 m.
            case BrnGameState::GameStateModuleIO::E_ACTION_ONCOMING:
            {
                const BrnGameState::GameStateModuleIO::OncomingAction* lpOncoming =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::OncomingAction*>(lpAction);
                OncomingEventWire388 lEvent;
                lEvent.mfDistance = lpOncoming->mfDistance;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 176  E_ACTION_TAILGATING (8 bytes) -> GUI 389 (4 bytes) --------------------
            case BrnGameState::GameStateModuleIO::E_ACTION_TAILGATING:
            {
                const BrnGameState::GameStateModuleIO::TailgatingAction* lpTailgating =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::TailgatingAction*>(lpAction);
                TailgatingEventWire389 lEvent;
                lEvent.mfDistance = lpTailgating->mfDistance;   // the index is NOT forwarded
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 178  AFTERTOUCH (12-byte action) -> GUI 403 (32-byte raw event) ----------------
            // ARTIST writes {sideways, forward, zero-qword, race-car index} at offsets 0/4/8/16;
            // the final 12 bytes of the 32-byte event remain stack residue.
            case BrnGameState::GameStateModuleIO::E_ACTION_AFTERTOUCH:
            {
                const BrnGameState::GameStateModuleIO::AftertouchAction* lpAftertouchAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::AftertouchAction*>(lpAction);
                BrnGui::GuiAftertouchEvent lEvent;
                lEvent.mfSidewaysAftertouch = lpAftertouchAction->mfSidewaysAftertouch;
                lEvent.mfForwardAftertouch = lpAftertouchAction->mfForwardAftertouch;
                lEvent.muZero = 0;
                lEvent.miRaceCarIndex = lpAftertouchAction->meRaceCarIndex;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 59  E_ACTION_ON_STUNT_ELEMENT_COMPLETE_FOR_COUNTY (8 bytes) --------------
            // @0x823EB98C..0x823EB9B0: { meStuntElementType@+0x00 <- Map(action+0x00),
            //                            meCounty@+0x04 <- action+0x04 }. No autosave request.
            case BrnGameState::GameStateModuleIO::E_ACTION_ON_STUNT_ELEMENT_COMPLETE_FOR_COUNTY:
            {
                const BrnGameState::GameStateModuleIO::OnStuntElementCompleteForCountyAction* lpAreaDone =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::OnStuntElementCompleteForCountyAction*>(lpAction);

                BrnGui::GuiEventStuntAreaComplete lEvent;               // id 219, 8 bytes
                lEvent.meStuntElementType =
                    MapStuntEnumsFromGameplayToGui(static_cast<u32>(lpAreaDone->meStuntElementType));
                lEvent.meCounty           = lpAreaDone->meCounty;
                PushGuiEvent(lEvent, lpGuiInput);

                if ( sbPropDiag && siDiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
                {
                    --siDiagLinesLeft;
                    *CgsDev::Log::gpDebugPrint
                        << "[UI-gate] gui-event id=" << lEvent.GetEventType()
                        << " type=" << static_cast<s32>(lEvent.meStuntElementType)
                        << " county=" << static_cast<s32>(lEvent.meCounty) << "\n";
                }
                break;
            }

            // ---- 60  E_ACTION_ON_STUNT_ELEMENT_COMPLETE_BY_TYPE (4 bytes) ----------------
            // @0x823EB9B8..0x823EB9D4: { meStuntElementType@+0x00 <- Map(action+0x00) }.
            case BrnGameState::GameStateModuleIO::E_ACTION_ON_STUNT_ELEMENT_COMPLETE_BY_TYPE:
            {
                const BrnGameState::GameStateModuleIO::OnStuntElementCompleteByTypeAction* lpAllDone =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::OnStuntElementCompleteByTypeAction*>(lpAction);

                BrnGui::GuiEventStuntAllComplete lEvent;                // id 220, 4 bytes
                lEvent.meStuntElementType =
                    MapStuntEnumsFromGameplayToGui(static_cast<u32>(lpAllDone->meStuntElementType));
                PushGuiEvent(lEvent, lpGuiInput);

                if ( sbPropDiag && siDiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
                {
                    --siDiagLinesLeft;
                    *CgsDev::Log::gpDebugPrint
                        << "[UI-gate] gui-event id=" << lEvent.GetEventType()
                        << " type=" << static_cast<s32>(lEvent.meStuntElementType) << "\n";
                }
                break;
            }

            // ---- 55  E_ACTION_REQUEST_AUTOSAVE (1 byte) ----------------------------------
            // ⭐⭐ [profile-save 2026-08-27] THE PROFILE-AUTOSAVE REQUEST. X360 @0x823EB818:
            //     case 55:
            //       HIBYTE(v282) = *v7;                                  // the action's own byte
            //       AddGuiEvent<BrnGui::GuiAutosaveRequestEvent>(module+7252512, &v282, input);
            //       goto LABEL_512;
            // -- a straight one-byte relay of the action payload onto GUI id 356. This is the
            // console's ONE general-purpose "the profile just changed, save it" path: the
            // producers are CarSelectManager's exit state (already live in this build --
            // BrnCarSelectManager.cpp posts KI_ACTION_AUTOSAVE on car confirm),
            // DriveThruManager::ProcessDriveThru (x2), DriveThruManager::UnlockCarChallengeForCar
            // and StreetManager::ProcessNewRoadScore. The consumer is GuiModule::Update's case
            // 356, which raises the module's autosave-pending latch; its tail then runs the
            // 60-second throttle and calls ProfileManager::Autosave.
            // The banner above names this arm as the nearest sibling for whoever came next.
            case BrnGameState::GameStateModuleIO::E_ACTION_REQUEST_AUTOSAVE:
            {
                BrnGui::GuiAutosaveRequestEvent lEvent;
                lEvent.maData[0] = *reinterpret_cast<const u8*>(lpAction);   // the action's own byte
                PushGuiEvent(lEvent, lpGuiInput);

                if ( sbPropDiag && siDiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
                {
                    --siDiagLinesLeft;
                    *CgsDev::Log::gpDebugPrint
                        << "[profile-save] action 55 -> gui 356 (flag "
                        << static_cast<s32>(lEvent.maData[0]) << ")\n";
                }
                break;
            }

            // ---- 6  E_ACTION_SET_TAKEDOWN_CAMERA_STATE -> GUI 377 payloads 2 / 3 ---------
            // ⭐⭐ [takedown HUD wave 2026-09-13] THE MISSING HALF OF GUI EVENT 377. The world
            // bridge (BridgeWorldVehicleDataToGui) posts only the two CRASH payloads,
            // 0 START_CRASHED / 1 LEAVE_CRASHED, on the player-crashing edge. The two TAKEDOWN
            // payloads, 2 START_TAKEDOWN / 3 LEAVE_TAKEDOWN, come from HERE and nowhere else --
            // those are the image's only two AddGuiEvent<GuiPlayerCrashingStateChangeEvent>
            // sites. With this arm absent, payload 2 was never posted anywhere in the build, so
            // HudMessageAnalyzer::HandleCrashedEvent's START_TAKEDOWN case could not run: the
            // takedown line parked by HandleTakedown (mbTakedownMessagePending +
            // mPendingTakedownEvent) had no flush and the "TDGdShutD" shutdown line never fired.
            // It also left the analyzer's meCrashEntryState unable to reach 2, which is the test
            // HandleTakedown uses to decide "fire immediately" instead of "park".
            //
            // The console arm, transcribed: it reads ONE byte -- the action's mbActive at +0x04
            // -- and posts `2 + (mbActive == 0)`:
            //     lbz r11, 4(record) ; cntlzw ; extrwi 1,26 ; addi r11, r11, 2 ; stw
            // i.e. it ignores the victim index and the signature/revenge flags entirely; only
            // the director's own case-6 arm consumes those. The pairing is exact, because the
            // SAME action drives both: TakedownManager::StartTakedownCamera posts active = 1
            // (-> 2) and EndTakedownCamera / ClearAllTakedowns post active = 0 (-> 3), so the
            // bar's takedown enter/leave edges cannot drift from the camera's.
            case BrnGameState::GameStateModuleIO::E_ACTION_SET_TAKEDOWN_CAMERA_STATE:      // 6
            {
                const BrnGameState::GameStateModuleIO::SetTakedownCameraAction* lpTakedownCamera =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::SetTakedownCameraAction*>(lpAction);

                BrnGui::GuiPlayerCrashingStateChangeEvent lEvent;
                lEvent.meCurrentState =
                    lpTakedownCamera->mbActive
                        ? BrnGui::GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_START_TAKEDOWN
                        : BrnGui::GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_LEAVE_TAKEDOWN;
                PushGuiEvent(lEvent, lpGuiInput);

                // [DIAG] NOT IN THE BINARY -- the takedown HUD chain's bridge rung.
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[takedown-hud] action 6 -> gui 377 state "
                        << static_cast<s32>(lEvent.meCurrentState)
                        << (lpTakedownCamera->mbActive ? " (START_TAKEDOWN)" : " (LEAVE_TAKEDOWN)")
                        << "\n";
                }
                break;
            }

            // ---- 120 -> GUI 373, A FREE-BURN RIVAL WAS SHUT DOWN -------------------------------
            // [FX-FLOW 2026-09-24, crash-parity G13-X5 remainder] ARTIST 0x823ED88C..0x823ED8A0
            // (jpt_823EA1F0 entry 120):
            //     ld   r11, 0(r31)        ; ShutdownAction::mVictimCarID (record +0)
            //     std  r11, var_35D8      ; the GuiShutdownEvent's one member
            //     bl   AddGuiEvent<BrnGui::GuiShutdownEvent>   -> AddEvent(q, rec, 373, 8)
            // Producer: TakedownManager::ProcessTakedownEvent @0x823940AC..0x823940D0 (action 120,
            // size 24). Consumers, in the GUI's own order: GuiCache::RecEvent case 373 stores the id
            // (mShutdownCarID, 0x8250FFA8) -- forwarded by GuiModule::DispatchInboundGuiEvents ahead
            // of the flow routing -- then BrnGui::InGame's case 373 shuts the HUD down and sends
            // "TO_RVL_POST" into BrnGui::OfflineRivalShutdown (e3b62101), whose presentation reads
            // the id back through GetShutdownCarID(). The arm was held back until both of those
            // existed (run_rcem2_rival_shutdown.py guards the order).
            case BrnGameState::GameStateModuleIO::E_ACTION_SHUTDOWN:   // 120
            {
                const BrnGameState::GameStateModuleIO::ShutdownAction* lpShutdownAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::ShutdownAction*>(lpAction);
                ShutdownEventWire373 lEvent;
                lEvent.mVictimCarID = lpShutdownAction->mVictimCarID;   // `ld 0(r31)` -> `std`
                PushGuiEvent(lEvent, lpGuiInput);

                // [DIAG] BRN_TD_DIAG -- NOT IN THE X360 BINARY.
                static const bool sbTdShutdownDiag = (getenv("BRN_TD_DIAG") != 0);
                if (sbTdShutdownDiag && CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "[td-gui] action 120 -> gui 373 (victim car id "
                                               << static_cast<u32>(lEvent.mVictimCarID >> 32) << ":"
                                               << static_cast<u32>(lEvent.mVictimCarID) << ")\n";
                }
                break;
            }

            // ---- 121 -> GUI 374, THE FREE-BURN RIVAL SHUTDOWN IS FINISHED --------------------
            // [crash-parity 2026-09-22] ARTIST 0x823ED8A8..0x823ED8B4 (jpt_823EA1F0 entry 121 at
            // 0x823EA3D8):
            //     mr r5, r20 ; addi r4, r1, 0xDD ; add r3, r29, r30
            //     bl AddGuiEvent<BrnGui::GuiShutdownFinishedEvent>     -> AddEvent(q, rec, 374, 1)
            // The record is one stack byte the arm never writes, and it does not read the
            // action. The byte's only reader-side consumer, GuiCache::RecEvent case 374
            // (0x8250FFC4..0x8250FFC8), stores mbCarUnlockPending = 1 without reading it, so a
            // zero byte stands in for the residue. Producer: TakedownManager::EndTakedownCamera's
            // free-burn arm (action 121, after the car is handed back to the player).
            // [FX-FLOW 2026-09-24] The consumer half is on PC now: GuiCache::RecEvent case 374 and
            // its GuiModule::DispatchInboundGuiEvents forward.
            case BrnGameState::GameStateModuleIO::E_ACTION_SHUTDOWN_FINISHED:   // 121
            {
                ShutdownFinishedEventWire374 lEvent;
                lEvent.mu8Unwritten = 0;
                PushGuiEvent(lEvent, lpGuiInput);

                // [DIAG] BRN_TD_DIAG -- NOT IN THE X360 BINARY.
                static const bool sbTdDiag = (getenv("BRN_TD_DIAG") != 0);
                if (sbTdDiag && CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "[td-gui] action 121 -> gui 374 (1 byte)\n";
                }
                break;
            }

            // ---- 222..224  original one-byte leftovers and the joined-car wire ------------------
            // ARTIST 0x823EC57C..0x823EC5C0. 222 and 224 pass unwritten stack bytes to one-byte
            // GUI 151 / 152 AddEvent helpers. Preserve that residue. 223 copies only action+0
            // (qword) and action+0x18 (one byte) into GUI 150; output bytes +9..+15 stay residue.
            // The upstream action producer and its declared payload size remain unlocated.
            case 222:
            {
                static_assert(sizeof(BrnGui::GuiEventCarEliminatedFromEvent) == 1,
                              "ARTIST GUI 151 tag is one byte");
                BrnGui::GuiEventCarEliminatedFromEvent lEvent;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case 223:
            {
                static_assert(sizeof(bool) == 1,
                              "ARTIST GUI 150 camera flag is one byte");
                const u8* lpActionBytes = reinterpret_cast<const u8*>(lpAction);
                BrnGui::GuiEventCarJoinedEvent lEvent;
                std::memcpy(&lEvent.mJoiningRivalID, lpActionBytes, sizeof(lEvent.mJoiningRivalID));
                // Preserve the raw byte as the lbz/stb pair does; do not normalize it through bool.
                std::memcpy(&lEvent.mbDoCamera, lpActionBytes + 0x18, sizeof(lEvent.mbDoCamera));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case 224:
            {
                GuiEvent152UnwrittenByte lEvent;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 242 / 245 / 246 / 247 / 248 / 250  THE RACE-MODE HUD MESSAGES -----------------
            // [FX-FLOW 2026-09-24, G11-D1 remainder] HUDMessageLogic's race arm posts these into the
            // game-action queue (242 GenerateFirstOrLastMessage, 245 GenerateLeaderMessages, 247
            // GenerateFinisherMessage, 248 GenerateRivalCheckpointMessage, 250 DetectCrashes /
            // DetectOnlineCrashes); 246 has no producer in the image. The console arms, jpt_823EA1F0:
            //   242 @0x823ED9F4  assert "lpTookLeadAction" (GameBridgeGameStateToX.cpp:0xEAF) ;
            //                    `lwz 8` -> +8, `ld 0` -> +0   AddGuiEvent<GuiTookLeadEvent>       484/16
            //   245 @0x823EC034  `ld 0` / `lfs 8` / `lbz 0x10` / `lwz 0xC` to the same offsets
            //                                                  AddGuiEvent<GuiInEventLeaderSplit>  420/24
            //   246 @0x823EC068  an unwritten stack byte       AddGuiEvent<GuiInEventNeckAndNeck>  421/1
            //   247 @0x823EC07C  `lwz 0` / `lwz 4`             AddGuiEvent<GuiInEventFinisher>     423/8
            //   248 @0x823EC0A0  `ld 8` / `ld 0` / `lwz 0x10`  AddGuiEvent<GuiInEventRivalProgress> 422/24
            //   250 @0x823EC0CC  `ld 0` / `lwz 8`              AddGuiEvent<GuiNetworkPlayerCrashingEvent> 482/16
            // 243 (took last), 244 (distance to finish) and 249 (player checkpoint) are in the
            // table's DEFAULT list: the console drops them here, so they post nothing on PC either.
            // GUI consumers: HudMessageAnalyzer::Update -- 484 HandleTookLead ("AggDrTkLead"), 420
            // HandleEventLeaderSplitTime ("EventCheckPt" offline), 421 "EventNeck", 423
            // HandleEventFinisher (online only), 482 HandleNetworkPlayerCrashedEvent (online only);
            // nothing reads 422.
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_TOOK_LEAD:   // 242
            {
                CGS_ASSERT(lpAction != 0, "lpTookLeadAction");   // GameBridgeGameStateToX.cpp:3759
                const BrnGameState::GameStateModuleIO::HUDMessageTookLeadAction* lpTookLeadAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageTookLeadAction*>(lpAction);
                BrnGui::GuiTookLeadEvent lEvent;
                lEvent.meLeadActiveRaceCarIndex = lpTookLeadAction->meActiveRaceCarIndex;   // `lwz 8`
                lEvent.mOfflineRivalCarID       = lpTookLeadAction->mCarId;                 // `ld 0`
                PushGuiEvent(lEvent, lpGuiInput);
                RaceHudBridgeDiag(242, 484, static_cast<s32>(lEvent.meLeadActiveRaceCarIndex), 0.0f);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_LEADING:   // 245
            {
                const BrnGameState::GameStateModuleIO::HUDMessageLeadingAction* lpLeadingAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageLeadingAction*>(lpAction);
                BrnGui::GuiInEventLeaderSplit lEvent;
                lEvent.mfLeadTime                 = lpLeadingAction->mfLeadTime;               // `lfs 8`
                lEvent.mLeadersCarID              = lpLeadingAction->mLeadingCarID;            // `ld 0`
                lEvent.mbLocalPlayerIsLeading     = lpLeadingAction->mbLocalPlayerIsLeading;   // `lbz 0x10`
                lEvent.meLeaderActiveRaceCarIndex = lpLeadingAction->meLeadingCarIndex;        // `lwz 0xC`
                PushGuiEvent(lEvent, lpGuiInput);
                RaceHudBridgeDiag(245, 420, static_cast<s32>(lEvent.meLeaderActiveRaceCarIndex), lEvent.mfLeadTime);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_NECK_AND_NECK:   // 246
            {
                NeckAndNeckEventWire421 lEvent;
                lEvent.mu8Unwritten = 0;
                PushGuiEvent(lEvent, lpGuiInput);
                RaceHudBridgeDiag(246, 421, -1, 0.0f);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_X_FINISHES:   // 247
            {
                const BrnGameState::GameStateModuleIO::HUDMessageXFinishesAction* lpFinishesAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageXFinishesAction*>(lpAction);
                BrnGui::GuiInEventFinisher lEvent;
                lEvent.meActiveRaceCarIndex = lpFinishesAction->meRivalRaceCarIndex;   // `lwz 0`
                lEvent.miFinishPosition     = lpFinishesAction->miFinishPosition;      // `lwz 4`
                PushGuiEvent(lEvent, lpGuiInput);
                RaceHudBridgeDiag(247, 423, static_cast<s32>(lEvent.meActiveRaceCarIndex),
                                  static_cast<f32>(lEvent.miFinishPosition));
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_X_REACHES_CHECKPOINT:   // 248
            {
                const BrnGameState::GameStateModuleIO::HUDMessageXReachesCheckpointAction* lpCheckpointAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageXReachesCheckpointAction*>(lpAction);
                BrnGui::GuiInEventRivalProgress lEvent;
                lEvent.mLandmarkID                = lpCheckpointAction->mLandmarkID;            // `ld 8`
                lEvent.mRivalID                   = lpCheckpointAction->mRivalID;               // `ld 0`
                lEvent.meRivalActiveRaceCarIndex  = lpCheckpointAction->meRivalRaceCarIndex;    // `lwz 0x10`
                PushGuiEvent(lEvent, lpGuiInput);
                RaceHudBridgeDiag(248, 422, static_cast<s32>(lEvent.meRivalActiveRaceCarIndex), 0.0f);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_X_CRASHES:   // 250
            {
                const BrnGameState::GameStateModuleIO::HUDMessageXCrashesAction* lpCrashesAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageXCrashesAction*>(lpAction);
                BrnGui::GuiNetworkPlayerCrashingEvent lEvent;
                lEvent.mRivalCarID                        = lpCrashesAction->mRivalID;               // `ld 0`
                lEvent.meNetworkPlayerActiveRaceCarIndex  = lpCrashesAction->meRivalRaceCarIndex;    // `lwz 8`
                PushGuiEvent(lEvent, lpGuiInput);
                RaceHudBridgeDiag(250, 482, static_cast<s32>(lEvent.meNetworkPlayerActiveRaceCarIndex), 0.0f);
                break;
            }

            // ---- 252..254  Burning Home Run and team-change HUD records -------------------------
            // ARTIST 0x823EC0F0..0x823EC230: preserve the three source assertions and raw payloads.
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_BHR_CHECKPOINT:
            {
                CGS_ASSERT(lpAction != 0, "lpHUDMessageBHRCheckpoint");
                const auto* lpBHRAction = reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageBHRCheckpointAction*>(lpAction);
                BrnGui::GuiBHRCheckpointReachedEvent lEvent;
                std::memcpy(&lEvent, lpBHRAction, sizeof(lEvent));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_BHR_RUNNER_CRASHED:
            {
                CGS_ASSERT(lpAction != 0, "lpHUDMessageBHRRunnerCrashesEvent");
                const auto* lpBHRAction = reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageBHRRunnerCrashedAction*>(lpAction);
                BrnGui::GuiHUDMessageBHRRunnerCrashed lEvent;
                std::memcpy(&lEvent, lpBHRAction, sizeof(lEvent));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_ONLINE_TEAM_CHANGE:
            {
                CGS_ASSERT(lpAction != 0, "lpHUDMessageOnlineTeamChangeAction");
                const auto* lpTeamChangeAction = reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageOnlineTeamChangeAction*>(lpAction);
                BrnGui::GuiOnlineTeamChangeEvent lEvent;
                std::memcpy(&lEvent, lpTeamChangeAction, sizeof(lEvent));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 256..260  rivalry status records -----------------------------------------------
            // ARTIST loads two CgsIDs at +0/+8, then appends the status values 0..3 for GUI 439;
            // the fleeing case forwards the same pair to GUI 440 without an added status word.
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_RIVAL_FIRST_HIT:
            {
                const auto* lpRivalAction = reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageFirstHitOnRivalAction*>(lpAction);
                BrnGui::GuiRivalryStatusChange lEvent;
                lEvent.mRivalID = lpRivalAction->mRivalID;
                lEvent.mCarID = lpRivalAction->mCarID;
                lEvent.meNewStatus = BrnGui::GuiRivalryStatusChange::E_RIVAL_LEVEL_NEW;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_RIVAL_IS_A_RIVAL:
            {
                const auto* lpRivalAction = reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageRivalIsARivalAction*>(lpAction);
                BrnGui::GuiRivalryStatusChange lEvent;
                lEvent.mRivalID = lpRivalAction->mRivalID;
                lEvent.mCarID = lpRivalAction->mCarID;
                lEvent.meNewStatus = BrnGui::GuiRivalryStatusChange::E_RIVAL_LEVEL_RIVAL;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_RIVAL_IS_A_TARGET:
            {
                const auto* lpRivalAction = reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageRivalIsATargetAction*>(lpAction);
                BrnGui::GuiRivalryStatusChange lEvent;
                lEvent.mRivalID = lpRivalAction->mRivalID;
                lEvent.mCarID = lpRivalAction->mCarID;
                lEvent.meNewStatus = BrnGui::GuiRivalryStatusChange::E_RIVAL_LEVEL_TARGET;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_RIVAL_IS_FLEEING:
            {
                const auto* lpRivalAction = reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageRivalIsFleeingAction*>(lpAction);
                BrnGui::GuiRivalIsFleeing lEvent;
                lEvent.mRivalID = lpRivalAction->mRivalID;
                lEvent.mCarID = lpRivalAction->mCarID;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_RIVAL_IS_WRECKED:
            {
                const auto* lpRivalAction = reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageRivalIsWreckedAction*>(lpAction);
                BrnGui::GuiRivalryStatusChange lEvent;
                lEvent.mRivalID = lpRivalAction->mRivalID;
                lEvent.mCarID = lpRivalAction->mCarID;
                lEvent.meNewStatus = BrnGui::GuiRivalryStatusChange::E_RIVAL_LEVEL_WRECKED;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 266..271  Online Stunt Run HUD records ------------------------------------------
            // The producer's 12-byte StuntRunTeamMessage maps by offset to the three typed GUI
            // receiver records; only bytes 0..9 are written by ARTIST and the two tail bytes remain
            // stack residue. Cases 270/271 build the receiver's score/time triple explicitly.
            case BrnGameState::HUDMessageLogic::E_HUD_MESSAGE_STUNT_RUN_ELIMINATION:
            {
                CGS_ASSERT(lpAction != 0, "lpEliminationAction");
                const auto* lpMessage = reinterpret_cast<const BrnGameState::HUDMessageLogic::StuntRunTeamMessage*>(lpAction);
                BrnGui::GuiNetworkStuntRunEliminationEvent lEvent;
                lEvent.mbIsLocalPlayer = lpMessage->miTeam;
                lEvent.mPlayerId = lpMessage->mNetworkPlayerID;
                std::memcpy(&lEvent.mbIsTeammate, &lpMessage->mbLocalPlayerOnTeam, sizeof(bool));
                std::memcpy(&lEvent.mbIsLastPlayer, &lpMessage->mbLocalPlayerIsSubject, sizeof(bool));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::HUDMessageLogic::E_HUD_MESSAGE_STUNT_RUN_LEADING:
            {
                CGS_ASSERT(lpAction != 0, "lpLeadingAction");
                const auto* lpMessage = reinterpret_cast<const BrnGameState::HUDMessageLogic::StuntRunTeamMessage*>(lpAction);
                BrnGui::GuiNetworkStuntRunLeadingEvent lEvent;
                lEvent.mbIsLocalPlayer = lpMessage->miTeam;
                lEvent.mPlayerId = lpMessage->mNetworkPlayerID;
                std::memcpy(&lEvent.mbIsTeammate, &lpMessage->mbLocalPlayerOnTeam, sizeof(bool));
                std::memcpy(&lEvent.mbIsLastPlayer, &lpMessage->mbLocalPlayerIsSubject, sizeof(bool));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::HUDMessageLogic::E_HUD_MESSAGE_STUNT_RUN_VICTORY:
            {
                CGS_ASSERT(lpAction != 0, "lpVictoryAction");
                const auto* lpMessage = reinterpret_cast<const BrnGameState::HUDMessageLogic::StuntRunTeamMessage*>(lpAction);
                BrnGui::GuiNetworkStuntRunVictoryEvent lEvent;
                lEvent.mbIsLocalPlayer = lpMessage->miTeam;
                lEvent.mPlayerId = lpMessage->mNetworkPlayerID;
                std::memcpy(&lEvent.mbIsTeammate, &lpMessage->mbLocalPlayerOnTeam, sizeof(bool));
                std::memcpy(&lEvent.mbIsLastPlayer, &lpMessage->mbLocalPlayerIsSubject, sizeof(bool));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::HUDMessageLogic::E_HUD_MESSAGE_STUNT_RUN_COMBO_END:
            {
                // ARTIST posts one unwritten byte as GUI 490; the GUI receiver is pointer-only.
                BrnGui::GuiNetworkLastStunRunEvent lEvent;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::HUDMessageLogic::E_HUD_MESSAGE_STUNT_RUN_SCORE:
            {
                CGS_ASSERT(lpAction != 0, "lpMessageAction");
                const auto* lpMessage = reinterpret_cast<const BrnGameState::HUDMessageLogic::StuntRunScoreMessage*>(lpAction);
                BrnGui::GuiNetworkSuntRunInfoMessageEvent lEvent;
                lEvent.mPlayerId = lpMessage->mNetworkPlayerID;
                lEvent.meMessageType = BrnGui::GuiOnlineStuntRunMessageEvent::E_ONLINE_STUNT_RUN_MESSAGE_SCORE;
                lEvent.miValue = lpMessage->miScore;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::HUDMessageLogic::E_HUD_MESSAGE_STUNT_RUN_TIME:
            {
                CGS_ASSERT(lpAction != 0, "lpMessageAction");
                const auto* lpMessage = reinterpret_cast<const BrnGameState::HUDMessageLogic::StuntRunTimeMessage*>(lpAction);
                BrnGui::GuiNetworkSuntRunInfoMessageEvent lEvent;
                lEvent.mPlayerId = -1;
                lEvent.meMessageType = BrnGui::GuiOnlineStuntRunMessageEvent::E_ONLINE_STUNT_RUN_MESSAGE_TIME;
                lEvent.miValue = static_cast<s32>(lpMessage->mfWarningTimeSeconds); // ARTIST fctiwz (30.0 -> 30)
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            // ---- 112  the DISTRICT CHANGE (8 bytes: {county, district}) -------------------
            // ⭐ [H1 district wave 2026-08-25] X360 case 112 @0x823EA-range (h1_dump2.txt),
            // verbatim: copy the action's 8-byte {county, district} pair, zero the third
            // word (the consumed flag), AddGuiEvent<GuiEventChangeDistrict> (id 169, 12B).
            // Producer: GameStateModule's case-115 arm; consumer: GuiCache::RecEvent case
            // 169 -> FBurnMainHudState's marker refresh (the HUD "you have entered
            // <district>" panel).
            case 112:
            {
                const s32* lpiRegion = reinterpret_cast<const s32*>(lpAction);

                BrnGui::GuiEventChangeDistrict lEvent;
                lEvent.meCounty    = lpiRegion[0];
                lEvent.meDistrict  = lpiRegion[1];
                lEvent.mu8Consumed = 0;
                lEvent.maPad[0] = lEvent.maPad[1] = lEvent.maPad[2] = 0;
                PushGuiEvent(lEvent, lpGuiInput);

                // [DIAG] NOT IN THE X360 BINARY -- the district chain's bridge rung.
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[district] action 112 -> gui 169 (county " << lpiRegion[0]
                        << " district " << lpiRegion[1] << ")\n";
                }
                break;
            }

            // ---- 148  the TRAINING TICKER (4 bytes: the BrnProgression::ETrainingType) ----
            // ⭐⭐ [tut-ticker] @0x823EA8C4..0x823EA930, instruction for instruction:
            //   lwz r3, 0(r31); cmpwi cr6, r3, 0x4D; bge default    -- type >= 77 -> drop
            //   bl ConvertTrainingTypeToStringId (r3 = the type -- Hex-Rays DROPPED this arg)
            //   beq default on NULL                                 -- no ticker string -> drop
            //   build the 2072-byte record: memset(strings, 0, 0x800); std 0 -> types[0..3];
            //     count(+0x810) = 0; flags(+0x811..814) = {0, 1, 1, 0}   (r19 == 0, r14 == 1)
            //   AddString(record, id, 2); AddGuiEvent<GuiEventTickerCustomMessage> -> id 537
            // Producer: TrainingManager::SendTrainingTickerMessage (GameAction 148); consumer:
            // CustomRendererManager::RecvEvent case 537 -> BlackBar + InGameMessage renderers.
            case 148:
            {
                const s32 liTrainingType = *reinterpret_cast<const s32*>(lpAction);
                if (liTrainingType < 77)
                {
                    const char* lpcStringId = ConvertTrainingTypeToStringId(
                        static_cast<BrnProgression::ETrainingType>(liTrainingType));
                    if (lpcStringId != 0)
                    {
                        TickerCustomMessageWire537 lEvent;
                        std::memset(&lEvent, 0, sizeof(lEvent));
                        lEvent.maFlags[1] = 1;   // +0x812 <- r14
                        lEvent.maFlags[2] = 1;   // +0x813 <- r14
                        lEvent.AddString(lpcStringId, 2);
                        PushGuiEvent(lEvent, lpGuiInput);

                        // [DIAG] NOT IN THE X360 BINARY -- the [tut-ticker] bridge rung
                        // (same first-N latch idiom as the [UI-gate] ladder, unconditional
                        // because this fires a handful of times per session at most).
                        static s32 siTickerDiagLeft = 8;
                        if (siTickerDiagLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
                        {
                            --siTickerDiagLeft;
                            *CgsDev::Log::gpDebugPrint
                                << "[tut-ticker] action 148 type=" << liTrainingType
                                << " -> gui 537 id='" << lpcStringId << "'\n";
                        }
                    }
                }
                break;
            }

            // ---- 97 / 98 / 100 / 101  THE DRIVE-THRU ON-SCREEN RESPONSE ------------------
            // ⭐⭐⭐ [drive-thru wave 2026-08-29] THIS IS WHAT A SUCCESSFUL DRIVE-THRU SAYS.
            // The mechanic has worked for a day (a gas station refills boost 35 -> 70 in one
            // session) and the game said nothing, because these four arms did not exist -- the
            // CONSUMER side has been complete and mounted the whole time:
            //   GUI 366 -> HudMessageAnalyzer::Update case 366 (BrnGuiHudMessageAnalyzer_wB_12
            //   .cpp) -> HandleDriveThrough @0x8251D570 (..._gUI_03.cpp) -> the three message
            //   tables in ..._wB_res.cpp: KAPC_DRIVE_THROUGH_MESSAGES ("DriThrGasStn",
            //   "DriThrBdyShp", ...), KAPC_DRIVE_THROUGH_MAGIC_MESSAGES (the rare flavour line,
            //   car wash + paint shop only) and KPAC_DRIVE_THROUGH_INEFFECTIVE_MESSAGES
            //   ("DriThrBdyShX") when mbEffective is false.
            // So the whole missing link was four `case` arms in an already-mounted, already-
            // running drain loop.
            //
            // X360 @0x823EB5C8..0x823EB644, instruction for instruction. All four build the
            // same 8-byte GuiDriveThroughEvent {meDriveThroughType@+0, mbEffective@+4} and post
            // it through AddGuiEvent<GuiDriveThroughEvent> (id 366):
            //   case 97  BODY_SHOP    lbz r11, 0x85(r31) ; stw r14(=1) ; stb r11
            //                         -> type 1, EFFECTIVE FROM THE PAYLOAD BYTE AT +0x85
            //   case 98  PAINT_SHOP   li r11,2 ; stb r14(=1) ; stw r11   -> type 2, effective 1
            //   case 100 GAS_STATION  li r11,3 ; stb r14(=1) ; stw r11   -> type 3, effective 1
            //   case 101 (stop pres)  li r11,5 ; stb r19(=0) ; stw r11   -> type 5, effective 0
            // ⚠️ ONLY THE BODY SHOP READS THE PAYLOAD. The other three carry compile-time
            // constants; do not "tidy" them into one shared arm that reads +0x85, because the
            // gas-station payload's +0x85 is zero and the message would become the FAILED one.
            //
            // ⓘ The body shop's +0x85 byte is a HARDCODED 1 at the producer
            // (DriveThruManager::ProcessDriveThru @0x8239BAA8 `li r26,1`, reproduced in
            // BrnDriveThruManager.cpp's PostShopAction as `lacPayload[133] = 1`). So on this
            // build the ineffective "DriThrBdyShX" line is unreachable THROUGH THIS PATH -- and
            // that is the console's own shape, not a gap here. Noted because a reader chasing
            // the ineffective message will otherwise suspect this arm.
            //
            // ⛔ Action 101 is E_ACTION_STOP_DRIVE_THRU_PRES -- posted by ProcessDriveThru's
            // `mbIsClosed` early-out, i.e. "this drive-thru is closed, cancel the presentation".
            // It maps to E_DRIVE_THROUGH_TYPE_FAILED, which is what makes the FAILED message a
            // reachable arm of HandleDriveThrough rather than dead table rows.
            case BrnGameState::GameStateModuleIO::E_ACTION_BODY_SHOP_DRIVE_THRU:     // 97
            case BrnGameState::GameStateModuleIO::E_ACTION_PAINT_SHOP_DRIVE_THRU:    // 98
            case BrnGameState::GameStateModuleIO::E_ACTION_GAS_STATION_DRIVE_THRU:   // 100
            case BrnGameState::GameStateModuleIO::E_ACTION_STOP_DRIVE_THRU_PRESENTATION: // 101
            {
                BrnGui::GuiDriveThroughEvent lEvent;   // id 366, 8 bytes

                if (liActionType ==
                    BrnGameState::GameStateModuleIO::E_ACTION_BODY_SHOP_DRIVE_THRU)
                {
                    lEvent.meDriveThroughType = BrnGui::GuiDriveThroughEvent::E_DRIVE_THROUGH_TYPE_BODY_SHOP;
                    // The 144-byte shop payload's per-action scalars start at +128; +0x85 (133)
                    // is the "the repair did something" byte the console reads back here.
                    lEvent.mbEffective =
                        (reinterpret_cast<const u8*>(lpAction)[KI_SHOP_ACTION_EFFECTIVE_BYTE] != 0);
                }
                else if (liActionType ==
                         BrnGameState::GameStateModuleIO::E_ACTION_PAINT_SHOP_DRIVE_THRU)
                {
                    lEvent.meDriveThroughType = BrnGui::GuiDriveThroughEvent::E_DRIVE_THROUGH_TYPE_PAINT_SHOP;
                    lEvent.mbEffective        = true;
                }
                else if (liActionType ==
                         BrnGameState::GameStateModuleIO::E_ACTION_GAS_STATION_DRIVE_THRU)
                {
                    lEvent.meDriveThroughType = BrnGui::GuiDriveThroughEvent::E_DRIVE_THROUGH_TYPE_GAS_STATION;
                    lEvent.mbEffective        = true;
                }
                else
                {
                    lEvent.meDriveThroughType = BrnGui::GuiDriveThroughEvent::E_DRIVE_THROUGH_TYPE_FAILED;
                    lEvent.mbEffective        = false;
                }

                PushGuiEvent(lEvent, lpGuiInput);

                // [DIAG] NOT IN THE X360 BINARY. Unconditional (a handful of lines per session
                // at most) and deliberately on the SAME `[drivethru]` tag as the producer's
                // GATE/ENTER/POST/GAS REFILL rungs, so one grep reads the whole ladder end to
                // end: an action that is posted but never bridged, and a bridged event whose
                // message never appears, are otherwise indistinguishable
                // [[diagnostics-that-lie]]. Delete with the rest of the drive-thru bring-up.
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[drivethru] BRIDGE action=" << liActionType
                        << " -> gui 366 type=" << static_cast<s32>(lEvent.meDriveThroughType)
                        << " effective=" << (lEvent.mbEffective ? 1 : 0) << "\n";
                }
                break;
            }

            // ---- 180  the GAME-STATS RESPONSE (432 bytes) ---------------------------------
            // ⭐⭐⭐ [pause-stats wave 2026-08-29] X360 case 180 @0x823EC8A0, the arm the Driver
            // Details pause panel's NUMBERS come out of. Unlike its 181 neighbour there is no
            // Construct call: the console builds the whole 432-byte record inline (~150
            // instructions of `lwz`/`stw` off the action, five `lwzx` off the module, four
            // Set::GetLength calls, two Profile::GetDriveThrusFound calls, five literals, then a
            // five-iteration loop that fills the six district columns) and hands it straight to
            // AddGuiEvent<GuiEventStatsResponse> (id 436, 432 bytes) @0x823ECC88.
            //
            // THE LAST HOP of the START-button pause screen's stat panel:
            //   GUI 435 -> game event 79 -> game action 180 -> GUI 436 (here). Without it
            // CrashNavDriverDetails::HandleStatData never runs and every stat field stays blank.
            //
            // ⚠️ THE ACTION RECORD *IS* A GameStats, not a wrapper -- see E_ACTION_GAME_STATS_
            // RESPONSE's banner in BrnGameActions.h. Named GameStats accessors replace the
            // console's raw `lwz <off>(r31)` throughout; every mapping below is the asm's.
            //
            // ⚠️ FOUR FIELDS ARE HARD-CODED CONSTANTS IN THE CONSOLE, AND THEY CROSS-CHECK:
            // `li 0xB / 0xE / 5 / 5` for the body-shop / gas-station / paint-shop / junkyard
            // totals, then `li 0x23` (35) for the drive-thru grand total -- and 11+14+5+5 == 35
            // exactly. They also equal the CAPACITIES of the four Profile drive-thru Sets whose
            // GetLength supplies the matching "found" counts (Set<CgsID,11>, <14>, <5>, <5>),
            // which is what pins each count to its category.
            //
            // ⚠️ SEVEN DESTINATION FIELDS ARE FED FROM THE MODULE, NOT THE ACTION (the console's
            // `lwzx r11, r29, <const>` reads, r29 == this): miCarsTotal and miDriversTot from
            // ProgressionManager::miMaxCarCount (+133468, the second halved with
            // `srawi 1 / addze`), the five "won" counters from the profile's
            // maGameModeTypeAmountCompletedSinceTheStart at modes 0/3/5/7/8, and
            // miBestRoadRageTakedownCount from Profile+118020.
            case BrnGameState::GameStateModuleIO::E_ACTION_GAME_STATS_RESPONSE:
            {
                const BrnGameState::GameStateModuleIO::GameStats* lpStats =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::GameStats*>(lpAction);

                typedef BrnGameState::GameStateModuleIO::GameStats GS;
                namespace GsmIO = BrnGameState::GameStateModuleIO;

                BrnProgression::ProgressionManager* lpProgressionManager =
                    GetGameStateModule().GetProgressionManager();
                const BrnProgression::Profile* lpProfile = lpProgressionManager->GetProfile();

                BrnGui::GuiEventStatsResponse lEvent;    // id 436, 432 bytes
                std::memset(&lEvent, 0, sizeof(lEvent));

                // the three ids (`ld`/`std` at +0x00/+0x08/+0x10)
                lEvent.mFaveCarId       = lpStats->GetValue(GS::E_ID_VALUE_TYPE_FAVOURITE_CAR);
                lEvent.mForgottenCarId  = lpStats->GetValue(GS::E_ID_VALUE_TYPE_FORGOTTEN_CAR);
                lEvent.mGreatestRivalId = lpStats->GetValue(GS::E_ID_VALUE_TYPE_NEMESIS);

                lEvent.miDistanceOnline  = lpStats->GetValue(GS::E_INT_VALUE_TYPE_DISTANCE_DRIVEN_ONLINE);
                lEvent.miDistanceOffline = lpStats->GetValue(GS::E_INT_VALUE_TYPE_DISTANCE_DRIVEN_OFFLINE);
                lEvent.miTimePlayed      = lpStats->GetValue(GS::E_INT_VALUE_TYPE_TIME_PLAYED);
                lEvent.miCarsCollected   = lpStats->GetValue(GS::E_INT_VALUE_TYPE_CARS_COLLECTED);
                // `lwzx r7, r29, 0x69598C` -- the module's own car total, and `srawi r7,r7,1 /
                // addze r8, r7` (a signed halving that rounds toward zero) for the drivers total.
                lEvent.miCarsTotal  = lpProgressionManager->GetMaxCarCount();
                lEvent.miDriversTot = lEvent.miCarsTotal / 2;
                lEvent.miDrivers    = 0;                                   // `stw r19` (r19 == 0)

                lEvent.miPowerParkingBest = lpStats->GetValue(GS::E_INT_VALUE_TYPE_BEST_POWER_PARKING);
                lEvent.miPowerParkingBest_BetweenOtherPlayers =
                    lpStats->GetValue(GS::E_INT_VALUE_TYPE_BEST_POWER_PARKING_BETWEEN_OTHER_PLAYERS);

                lEvent.miGolds   = lpStats->GetValue(GS::E_INT_VALUE_TYPE_MEDALS_GOLD);
                lEvent.miSilvers = lpStats->GetValue(GS::E_INT_VALUE_TYPE_MEDALS_SILVER);
                lEvent.miBronzes = lpStats->GetValue(GS::E_INT_VALUE_TYPE_MEDALS_BRONZE);
                // `add r6, r7, r8` then `add r8, r6, r8` -- silver + bronze + gold.
                lEvent.miAllMedalsEarned = lEvent.miGolds + lEvent.miSilvers + lEvent.miBronzes;

                // ⚠️ NOT A TYPO: the console writes the EVENT-medal total into BOTH the
                // all-medals total and the event-medals total (one `lwz r11, 0x40(r31)` feeding
                // `stw r11` at +0x38 AND at +0x40). Same register, two destinations.
                lEvent.miAllMedalsTotal      = lpStats->GetValue(GS::E_INT_VALUE_TYPE_TOTAL_EVENT_MEDALS);
                lEvent.miEventMedalsEarned   = lpStats->GetValue(GS::E_INT_VALUE_TYPE_NUM_EVENT_MEDALS);
                lEvent.miEventMedalsTotal    = lpStats->GetValue(GS::E_INT_VALUE_TYPE_TOTAL_EVENT_MEDALS);
                // and likewise the two road-rule pairs are each written twice (+0x44/+0x4C and
                // +0x48/+0x50 from the same two registers).
                lEvent.miRoadRuleMedalsEarned = lpStats->GetValue(GS::E_INT_VALUE_TYPE_NUM_ROAD_RULE_MEDALS);
                lEvent.miRoadRuleMedalsTotal  = lpStats->GetValue(GS::E_INT_VALUE_TYPE_TOTAL_ROAD_RULE_MEDALS);
                lEvent.miRoadRules            = lEvent.miRoadRuleMedalsEarned;
                lEvent.mRoadsRuledTotal       = lEvent.miRoadRuleMedalsTotal;

                lEvent.miJumps    = lpStats->GetValue(GS::E_INT_VALUE_TYPE_JUMPS);
                lEvent.miJumpTot  = lpStats->GetValue(GS::E_INT_VALUE_TYPE_JUMPS_MAX);
                lEvent.miSmashes  = lpStats->GetValue(GS::E_INT_VALUE_TYPE_SMASHES);
                lEvent.miSmashTot = lpStats->GetValue(GS::E_INT_VALUE_TYPE_SMASHES_MAX);
                lEvent.miStunts   = lpStats->GetValue(GS::E_INT_VALUE_TYPE_STUNTS);
                lEvent.miStuntTot = lpStats->GetValue(GS::E_INT_VALUE_TYPE_STUNTS_MAX);

                lEvent.miSignatureTDs    = 0;   // `stw r19`
                lEvent.miSignatureTDsTot = 0;   // `stw r19`

                lEvent.miTotalTakedowns    = lpStats->GetValue(GS::E_INT_VALUE_TYPE_TAKEDOWNS);
                lEvent.miStandardTakedowns = lpStats->GetTakedownTypeCount(0);
                lEvent.miVerticalTakedowns = lpStats->GetTakedownTypeCount(3);
                lEvent.miTBoneTakedowns    = lpStats->GetTakedownTypeCount(2);
                lEvent.miAftertouchTakedowns = 0;                       // `stw r19`
                lEvent.miCarTakedowns      = lpStats->GetTakedownTypeCount(10);
                lEvent.miVanTakedowns      = lpStats->GetTakedownTypeCount(11);
                lEvent.miBusTakedowns      = lpStats->GetTakedownTypeCount(12);
                lEvent.miBigRigTakedowns   = 0;                         // `stw r19`

                lEvent.mRoadsRuledTime     = lpStats->GetRoadsRuledCount(0);
                lEvent.mRoadsRuledCrash    = lpStats->GetRoadsRuledCount(1);
                lEvent.mRoadsRuledComplete = lpStats->GetValue(GS::E_INT_VALUE_TYPE_TOTALROADSRULED);
                lEvent.mNumberOfRoads      = lpStats->GetTotalRoads();

                lEvent.miWinsToNextRank = lpStats->GetValue(GS::E_INT_VALUE_TYPE_TOTAL_WINS_FOR_NEXT_RANK);
                lEvent.miCarsToShutdown = lpStats->GetValue(GS::E_INT_VALUE_TYPE_TOTAL_CARS_TO_SHUTDOWN);
                // `lfs f0, 0xA4(r31) / fctiwz / stfiwx` -- the float percentage is TRUNCATED to an
                // int here, which is why the panel shows a whole number.
                lEvent.miPercentageComplete =
                    static_cast<s32>(lpStats->GetValue(GS::E_FLOAT_VALUE_PERCENTAGE_COMPLETE));

                lEvent.miDriveThrusFound = lpProfile->GetDriveThrusFound();   // first of two calls

                // the five "won" counters -- Profile+336/+348/+356/+364/+368, i.e. modes
                // 0 / 3 / 5 / 7 / 8 of maGameModeTypeAmountCompletedSinceTheStart.
                lEvent.miRacesWon     = lpProfile->GetGameModeTypeCompletedSinceTheStart(GsmIO::E_MODE_OFFLINE_RACE);
                lEvent.miRoadRagesWon = lpProfile->GetGameModeTypeCompletedSinceTheStart(GsmIO::E_MODE_ROAD_RAGE);
                lEvent.miMarkedManWon = lpProfile->GetGameModeTypeCompletedSinceTheStart(GsmIO::E_MODE_MARKED_MAN);
                lEvent.miChallengesWon = lpProfile->GetGameModeTypeCompletedSinceTheStart(GsmIO::E_MODE_BURNING_ROUTE);
                lEvent.miStuntRunsWon = lpProfile->GetGameModeTypeCompletedSinceTheStart(GsmIO::E_MODE_STUNT_ATTACK);

                lEvent.miBestShowtime = lpStats->GetValue(GS::E_INT_VALUE_TYPE_BEST_SHOWTIME);
                lEvent.miBestRoadRageTakedownCount = lpProfile->GetHighestNumberOfTakeDownsInRoadRage();
                lEvent.miBestBoostChain = lpStats->GetValue(GS::E_INT_VALUE_TYPE_BEST_BOOST_CHAIN);
                lEvent.miBestDrift      = lpStats->GetValue(GS::E_INT_VALUE_TYPE_BEST_DRIFT);
                lEvent.miBestOncoming   = lpStats->GetValue(GS::E_INT_VALUE_TYPE_BEST_ONCOMING);
                // the ONLY two `stfs` in the arm -- these stay floats end to end.
                lEvent.mfBestAirtime = lpStats->GetValue(GS::E_FLOAT_VALUE_TYPE_BEST_AIRTIME);
                lEvent.mfBestSpin    = lpStats->GetValue(GS::E_FLOAT_VALUE_TYPE_BEST_SPIN);
                lEvent.miBestNumBarrelRolls = lpStats->GetValue(GS::E_INT_VALUE_TYPE_BEST_NO_BARREL_ROLLS);
                lEvent.miHighestStuntScore  = lpStats->GetValue(GS::E_INT_VALUE_TYPE_HIGHEST_STUNT_SCORE);
                lEvent.miEventsFound  = lpStats->GetValue(GS::E_INT_VALUE_TYPE_EVENTS_FOUND);
                lEvent.miTotalEvents  = lpStats->GetValue(GS::E_INT_VALUE_TYPE_EVENTS_TOTAL);

                // the four discovered-drive-thru Set lengths. The console inlines
                // Set<CgsID,11/14/5/5>::GetLength on Profile+42568/+42712/+42664/+42520, which is
                // exactly Profile::GetNumDriveThrusDiscovered's four non-car-park arms.
                lEvent.miBodyShopsFound   = lpProfile->GetNumDriveThrusDiscovered(BrnTrigger::GenericRegion::E_TYPE_BODY_SHOP);
                lEvent.miGasStationsFound = lpProfile->GetNumDriveThrusDiscovered(BrnTrigger::GenericRegion::E_TYPE_GAS_STATION);
                lEvent.miPaintShopsFound  = lpProfile->GetNumDriveThrusDiscovered(BrnTrigger::GenericRegion::E_TYPE_PAINT_SHOP);
                lEvent.miJunkYardsFound   = lpProfile->GetNumDriveThrusDiscovered(BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD);

                lEvent.miBodyShopsTotal   = 11;   // `li r11, 0xB`
                lEvent.miGasStationsTotal = 14;   // `li r11, 0xE`
                lEvent.miPaintShopsTotal  = 5;    // `li r11, 5`
                lEvent.miJunkYardsTotal   = 5;    // `li r11, 5`
                lEvent.miTotalDriveThrus  = 35;   // `li r11, 0x23`  (== 11 + 14 + 5 + 5)
                lEvent.miTotalDriveThrusFound = lpProfile->GetDriveThrusFound();   // second call

                // the six district columns, one district per iteration (the console walks ONE
                // word cursor over the action's two 3x5 grids and six destination bases).
                for (s32 liDistrict = 0;
                     liDistrict < BrnGui::GuiEventStatsResponse::KI_NUM_DISTRICTS;
                     ++liDistrict)
                {
                    lEvent.mBillboardStunts[liDistrict] =
                        lpStats->GetCurrentStuntElementPerCounty(BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD, liDistrict);
                    lEvent.mJumpStunts[liDistrict] =
                        lpStats->GetCurrentStuntElementPerCounty(BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP, liDistrict);
                    lEvent.mSmashStunts[liDistrict] =
                        lpStats->GetCurrentStuntElementPerCounty(BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH, liDistrict);
                    lEvent.mMaxBillboardStunts[liDistrict] =
                        lpStats->GetMaxStuntElementPerCounty(BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD, liDistrict);
                    lEvent.mMaxJumpStunts[liDistrict] =
                        lpStats->GetMaxStuntElementPerCounty(BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP, liDistrict);
                    lEvent.mMaxSmashStunts[liDistrict] =
                        lpStats->GetMaxStuntElementPerCounty(BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH, liDistrict);
                }

                PushGuiEvent(lEvent, lpGuiInput);

                // [DIAG] NOT IN THE X360 BINARY -- the stat panel's bridge rung, same
                // change-only idiom as the [ddetails] rank rung below. One line per screen entry.
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[ddetails] action 180 -> gui 436 (cars " << lEvent.miCarsCollected
                        << "/" << lEvent.miCarsTotal
                        << " events " << lEvent.miEventsFound << "/" << lEvent.miTotalEvents
                        << " drivethrus " << lEvent.miTotalDriveThrusFound << "/" << lEvent.miTotalDriveThrus
                        << " roads " << lEvent.mRoadsRuledComplete << "/" << lEvent.mNumberOfRoads
                        << " pct " << lEvent.miPercentageComplete
                        << " td " << lEvent.miTotalTakedowns << ")\n";
                }
                break;
            }

            // ---- 181  the RANK-PROGRESS RESPONSE (36 bytes) -------------------------------
            // ⭐⭐ [driver-details pause wave 2026-08-28] X360 case 181 @0x823ECC90, and it is
            // exactly two calls:
            //   0x823ECC90  mr   r4, r31                      ; the action record
            //   0x823ECC94  addi r3, r1, var_35C0             ; a stack-local event
            //   0x823ECC98  bl   GuiEventRankProgressResponse::Construct
            //   0x823ECC9C  mr   r5, r20                      ; lpGuiInput
            //   0x823ECCA0  addi r4, r1, var_35C0
            //   0x823ECCA4  add  r3, r29, r30                 ; the embedded GuiModule
            //   0x823ECCA8  bl   AddGuiEvent<GuiEventRankProgressResponse>   ; id 438, 36 bytes
            // Construct is the nine-word rotation documented in the event's own header.
            //
            // THIS IS THE LAST HOP of the START-button pause screen's licence ladder:
            //   GUI 437 -> game event 80 -> game action 181 -> GUI 438 (here). Without it
            // CrashNavDriverDetails::UpdateSetupLicense never advances past
            // E_INTERNALSTATE_SETUPLICENSE and the screen never loads its apt movie.
            // ---- 179  E_ACTION_EVENT_STATE_RESPONSE (1404 bytes) -> GUI event 556 --------------
            // X360 TranslateGameActionsToGuiEvents @0x823E9CE0 case 179: assert the record
            // (GameBridgeGameStateToX.cpp:3610), `memcpy(local, action, 1404)`, then
            // AddGuiEvent<GuiEventEventStateResponse> @0x823D85F8 == AddEvent(queue, local, 556,
            // 1404). The payload IS the Array<ProfileEvent,175> the game state built; GuiCache's
            // case 556 copies the same 1404 bytes into its profile-event array.
            case BrnGameState::GameStateModuleIO::E_ACTION_EVENT_STATE_RESPONSE:
            {
                CGS_ASSERT(lpAction != 0, "lpEventStateResponse");   // cpp:3610
                struct EventStateResponseWire556
                {
                    u8 mau8Payload[1404];
                    s32 GetEventType() const { return 556; }
                };
                static_assert(sizeof(EventStateResponseWire556) == 1404,
                              "X360 AddGuiEvent<GuiEventEventStateResponse> posts 1404 bytes (id 556)");
                EventStateResponseWire556 lEvent;
                std::memcpy(lEvent.mau8Payload, lpAction, sizeof(lEvent.mau8Payload));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            case BrnGameState::GameStateModuleIO::E_ACTION_RANK_INFO_RESPONSE:
            {
                const BrnGameState::GameStateModuleIO::RankInfoResponseAction* lpRankInfo =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::RankInfoResponseAction*>(lpAction);

                BrnGui::GuiEventRankProgressResponse lEvent;    // id 438, 36 bytes
                lEvent.Construct(lpRankInfo);
                PushGuiEvent(lEvent, lpGuiInput);

                // [DIAG] NOT IN THE X360 BINARY -- the licence ladder's bridge rung, same
                // change-only idiom as the [district] rung above.
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[ddetails] action 181 -> gui 438 (rank "
                        << lpRankInfo->miPlayerRank << " race " << lpRankInfo->miOfflineRace
                        << " rage " << lpRankInfo->miRoadRage
                        << " stunt " << lpRankInfo->miStuntAttack
                        << " marked " << lpRankInfo->miMarkedMan << ")\n";
                }
                break;
            }

            // ARTIST 0x823EBC10..0x823EBC48 posts the 64-byte action-182 image as GUI 406.
            // The first 56 bytes are the PlayerInfo payload itself (the GUI event's first 12
            // bytes are not a queue header here); the console then writes only damage at +0x38.
            case BrnGameState::GameStateModuleIO::E_ACTION_PLAYER_INFO_RESPONSE:
            {
                using PlayerInfoAction = BrnGameState::GameStateModuleIO::PlayerInfoResponseAction;
                static_assert(sizeof(PlayerInfoAction) == 64, "X360 action 182 payload size");
                static_assert(sizeof(BrnGui::GuiPlayerInfoResponse) == 64,
                              "X360 GUI 406 payload size");
                const auto& lrAction = *reinterpret_cast<const PlayerInfoAction*>(lpAction);
                BrnGui::GuiPlayerInfoResponse lEvent;
                std::memcpy(&lEvent, &lrAction.mPlayerInfo, sizeof(lrAction.mPlayerInfo));
                std::memcpy(reinterpret_cast<u8*>(&lEvent) + 0x38,
                            &lrAction.mbIsCarDamaged, sizeof(lrAction.mbIsCarDamaged));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 188 / 189  RIVALRY DATA RESPONSES ------------------------------------
            // X360 cases 188/189 at 0x823ECCB0/0x823ECE0C post GUI 442/688 and 444/32.
            // The GameState producers already post these typed records: SendAllRivalryData
            // (action 188, 1288 bytes) and ProcessGameEvents case 102 (action 189, 32 bytes).
            case BrnGameState::GameStateModuleIO::E_ACTION_ALL_RIVALRY_DATA_RESPONSE:
            {
                const BrnGameState::GameStateModuleIO::RivalryOverviewAction& lrAction =
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::RivalryOverviewAction*>(lpAction);
                BrnGui::GuiEventRivalryFullInfoResponse lEvent;   // GUI 442, 688 bytes; plain record

                // ARTIST compares signed count > 34, asserts at the original line, then still
                // iterates the supplied count. Keep that continuation and do not clamp it.
                if (lrAction.miRivalsReturned > BrnGui::GuiEventRivalryFullInfoResponse::KI_MAX_NUM_RIVALS_IN_EVENT)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "BrnGui::GuiEventRivalryFullInfoResponse::KI_MAX_NUM_RIVALS_IN_EVENT >= lpRivalOverview->miRivalsReturned",
                        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/GameBridgeGameStateToX.cpp",
                        3047);
                    CgsDev::Assert::EndAssert();
                }

                lEvent.miRivalsReturned = lrAction.miRivalsReturned;
                for (s32 liRival = 0; liRival < lrAction.miRivalsReturned; ++liRival)
                {
                    lEvent.maRivalIds[liRival]    = lrAction.mRivalIDs[liRival];
                    lEvent.maRivalCarIds[liRival] = lrAction.mRivalCarIDs[liRival];

                    s32 liGuiStatus;
                    bool lbGuiStatusWasWritten = true;
                    switch (lrAction.mRivalryStatus[liRival])
                    {
                    case BrnGameState::GameStateModuleIO::RivalryOverviewAction::E_RIVALRY_STAGE_UNKNOWN:
                        liGuiStatus = 1; break;
                    case BrnGameState::GameStateModuleIO::RivalryOverviewAction::E_RIVALRY_STAGE_DRIVER:
                        liGuiStatus = 2; break;
                    case BrnGameState::GameStateModuleIO::RivalryOverviewAction::E_RIVALRY_STAGE_RIVAL:
                        liGuiStatus = 3; break;
                    case BrnGameState::GameStateModuleIO::RivalryOverviewAction::E_RIVALRY_STAGE_TARGET:
                        liGuiStatus = 4; break;
                    case BrnGameState::GameStateModuleIO::RivalryOverviewAction::E_RIVALRY_STAGE_WRECKED:
                        liGuiStatus = 5; break;
                    case BrnGameState::GameStateModuleIO::RivalryOverviewAction::E_RIVALRY_STAGE_INVALID:
                        liGuiStatus = 0; break;
                    default:
                        // The original asserts with the numeric stage, continues, and does not
                        // store this output slot. Leave the stack bytes unwritten on this path.
                        CgsDev::Assert::BeginAssert();
                        {
                            CgsDev::StrStream lAssertStream(CgsDev::Assert::gpcMessageBuffer,
                                                            CgsDev::Assert::giMessageBufferSize);
                            lAssertStream << "Unknown rivalry type "
                                          << static_cast<s32>(lrAction.mRivalryStatus[liRival]) << "\n";
                        }
                        CgsDev::Assert::FireAssert(
                            CgsDev::Assert::gpcMessageBuffer,
                            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/GameBridgeGameStateToX.cpp",
                            3099);
                        CgsDev::Assert::EndAssert();
                        lbGuiStatusWasWritten = false;
                        break;
                    }
                    if (lbGuiStatusWasWritten)
                        lEvent.maRivalryStatus[liRival] = liGuiStatus;
                }

                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            case BrnGameState::GameStateModuleIO::E_ACTION_ONE_RIVALRY_DATA_RESPONSE:
            {
                const BrnGameState::GameStateModuleIO::RivalryOneInDepthAction& lrAction =
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::RivalryOneInDepthAction*>(lpAction);
                BrnGui::GuiEventRivalInfoResponse lEvent;   // GUI 444, 32 bytes; plain record

                lEvent.mRivalId      = lrAction.mId;
                lEvent.mCarId        = lrAction.mCarId;
                lEvent.meCountyIndex = lrAction.meCountyIndex;
                lEvent.miRaceWins    = lrAction.miRaceWins;

                // ARTIST sign-extends the s8 field before asserting, then stores that byte even
                // after the assertion; preserve both the full signed range check and raw value.
                if (lrAction.miProgressStatus < 0 || lrAction.miProgressStatus > 100)
                {
                    CgsDev::Assert::BeginAssert();
                    {
                        CgsDev::StrStream lAssertStream(CgsDev::Assert::gpcMessageBuffer,
                                                        CgsDev::Assert::giMessageBufferSize);
                        lAssertStream << "Invalid progress"
                                      << static_cast<s32>(lrAction.miProgressStatus) << "\n";
                    }
                    CgsDev::Assert::FireAssert(
                        CgsDev::Assert::gpcMessageBuffer,
                        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/GameBridgeGameStateToX.cpp",
                        3120);
                    CgsDev::Assert::EndAssert();
                }

                lEvent.miProgressStatus = lrAction.miProgressStatus;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // =====================================================================
            // ⭐⭐⭐ 127 / 128 / 139-144 -- THE SHOWTIME FAMILY (showtime-score wave 2026-08-29)
            // =====================================================================
            // jpt_823EA1F0's "cases 127,128,139-144" arm, @0x823ED714..0x823ED754, transcribed
            // instruction for instruction:
            //     lwz  r11, var_3534(r1)   ; the local set once in the prologue @0x823E9D84 to
            //                                `r31 + 0x30000 - 0x5B48` == lpGameStateOutput+173240
            //                                == OutputBuffer::GetScoringOutputInterface()
            //     lwz  r11, 0xA3C(r11)     ; +0xA3C is ScoringOutputInterface::meGameModeType
            //                                (the committed member run pins mePlayerRaceCarIndex
            //                                 at +0xA34 and miNumPlayersInGame at +0xA38, so the
            //                                 next word is meGameModeType -- and the sibling
            //                                 event-status slice already reads it by that name)
            //     cmpwi 2 / beq ; cmpwi 0x10 / bne -> default   ; the offline/online showtime pair
            //     mr r4, r3 ; mr r5, r31 ; mr r6, r20 ; mr r3, r29
            //     bl TranslateShowtimeActionToGuiEvent
            //
            // ⚠️ Read through the SCORING OUTPUT INTERFACE, not through
            // GetModeManager()->GetCurrentGameModeType(), even though both words track the same
            // mode: the console reads the published output-buffer copy at this seat, and the
            // ModeManager route would be a different member on a different object with a
            // different publish latency. Same rule the GUI-377 producer's own note states.
            //
            // ⛔ NOT MODE-GATED BY US TWICE: the arm below is the whole gate; the callee has none.
            case BrnGameState::GameStateModuleIO::E_ACTION_WORLD_STUNT_PERFORMED:   // 127
            case BrnGameState::GameStateModuleIO::E_ACTION_OVERHEAD_SIGN_HIT:       // 128
            case BrnGameState::GameStateModuleIO::E_ACTION_VEHICLE_LEAPT:           // 139
            case BrnGameState::GameStateModuleIO::E_ACTION_VEHICLE_HIT:             // 140
            case BrnGameState::GameStateModuleIO::E_ACTION_ENTER_NEW_ROAD:          // 141
            case BrnGameState::GameStateModuleIO::E_ACTION_SHOWTIME_UPDATE:         // 142
            case BrnGameState::GameStateModuleIO::E_ACTION_SHOWTIME_MODE_SWITCH:    // 143
            case BrnGameState::GameStateModuleIO::E_ACTION_JUST_BOUNCED:            // 144
            {
                const BrnGameState::GameStateModuleIO::ScoringOutputInterface* const lpScoring =
                    lpGameStateOutput->GetScoringOutputInterface();

                // The console does not null-test here (it computes the address inline off a
                // buffer it already asserted), so neither does this -- adding a test would be an
                // invented arm. The accessor returns the address of an embedded sub-object.
                const s32 liGameModeType = static_cast<s32>(lpScoring->meGameModeType);
                const bool lbShowtime =
                    (liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
                     liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);

                // [DIAG] NOT IN THE X360 BINARY. BRN_SHOWTIME_SCORE_DIAG, first-N latched.
                // ⭐ IT PRINTS ON BOTH SIDES OF THE GATE ON PURPOSE. A probe that only speaks
                // when the gate PASSES cannot tell "the action never arrived" from "the gate
                // refused it", and those two have completely different fixes -- one is an
                // unbodied producer upstream, the other is a wrong mode read here. The first
                // cut of this instrument printed only inside the arms and returned a zero that
                // meant nothing.
                {
                    static const bool sbGateDiag  = ( getenv( "BRN_SHOWTIME_SCORE_DIAG" ) != 0 );
                    static s32        siGateLines = 16;
                    if ( sbGateDiag && siGateLines > 0 && CgsDev::Log::gpDebugPrint != 0 )
                    {
                        --siGateLines;
                        *CgsDev::Log::gpDebugPrint
                            << "[showtime-gate] action " << liActionType
                            << " scoringOut.meGameModeType=" << liGameModeType
                            << (lbShowtime ? " -> TRANSLATE\n" : " -> refused (not showtime)\n");
                    }
                }

                if (lbShowtime)
                {
                    TranslateShowtimeActionToGuiEvent(liActionType, lpAction, lpGuiInput);
                }
                break;
            }

            // ARTIST 0x823ED570..0x823ED5C4: action294 is the image-save request that becomes
            // GUI358. The action producer widens its pointer to the host width; copy the whole
            // 24-byte ImageFileInfo-shaped record, including the unwritten trailing X360 word.
            case 294:
            {
                const BrnGameState::GameStateModuleIO::ImageSaveRequestAction* lpSaveImageAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::ImageSaveRequestAction*>(lpAction);
                CGS_ASSERT(lpSaveImageAction != 0, "lpSaveImageAction");
                BrnGui::GuiEventSaveImageFileAndAutosave lEvent;
                std::memcpy(&lEvent.mImageFileInfo, lpSaveImageAction, sizeof(lEvent.mImageFileInfo));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED5C8..0x823ED608: assert the action then copy the whole image-file
            // record array to GUI 359. The X360 3x16-byte record widens to 3x24 bytes here.
            case 295: // ImageManager::KI_EVENT_PREPARE_RENDER
            {
                CGS_ASSERT(lpAction != 0, "lpLoadImagesAction");
                BrnGui::GuiEventLoadImageFiles lEvent;
                std::memcpy(lEvent.maImageFileInfo, lpAction, sizeof(lEvent.maImageFileInfo));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 152..160  the free-burn challenge family (online free-burn lobby) ------------
            // The online time-out clock: the {seconds, fraction} record becomes GUI 108.
            case BrnGameState::GameStateModuleIO::E_ACTION_MODE_TIME_REMAINING:
            {
                CGS_ASSERT(lpAction != 0, "lpTimeOutTimer != NULL");
                OnlineTimeoutWire108 lEvent;
                lEvent.mfTimeLeft =
                    reinterpret_cast<const CgsSystem::Time*>(lpAction)->GetFloatVal();
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // The challenge lifecycle: begin -> 574, trigger -> 576, ended -> 108 (-1.0) + 578 +
            // 104, results finished -> 579 + 104. The other event types post nothing.
            case BrnGameState::GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE:
            {
                CGS_ASSERT(lpAction != 0, "lpChallengeAction");
                const BrnGameState::GameStateModuleIO::FreeburnChallengeAction* lpChallengeAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::FreeburnChallengeAction*>(lpAction);

                BrnNetHarnessPC::WitnessTag("netui", "fburn-gui", "action 153 type=%d id=%016llx host=%d",
                                            lpChallengeAction->meEventType,
                                            static_cast<unsigned long long>(lpChallengeAction->mChallengeID),
                                            lpChallengeAction->mbIsHost ? 1 : 0);

                switch (lpChallengeAction->meEventType)
                {
                case 0:
                {
                    BrnGui::GuiChallengeStartEvent lEvent;
                    lEvent.mChallengeID  = lpChallengeAction->mChallengeID;
                    lEvent.mbIsLocalHost = lpChallengeAction->mbIsHost;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 1:
                {
                    BrnGui::GuiChallengeTriggerResponse lEvent;
                    lEvent.mChallengeID  = lpChallengeAction->mChallengeID;
                    lEvent.mbIsLocalHost = lpChallengeAction->mbIsHost;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 5:
                {
                    OnlineTimeoutWire108 lTimeout;
                    lTimeout.mfTimeLeft = -1.0f;
                    PushGuiEvent(lTimeout, lpGuiInput);

                    BrnGui::GuiChallengeEndEvent lEvent;
                    lEvent.mChallengeID                  = lpChallengeAction->mChallengeID;
                    lEvent.meChallengeStatus             = lpChallengeAction->meChallengeStatus;
                    lEvent.miNumChallengesComplete       = lpChallengeAction->miNumChallengesComplete;
                    lEvent.miTotalNumChallenges          = lpChallengeAction->miTotalNumChallenges;
                    lEvent.mbAbortingToStartNewChallenge = lpChallengeAction->mbAbortingToStartNewChallenge;
                    PushGuiEvent(lEvent, lpGuiInput);

                    PushGuiTag(104, lpGuiInput);
                    break;
                }
                case 6:
                    PushGuiTag(579, lpGuiInput);
                    PushGuiTag(104, lpGuiInput);
                    break;
                default:
                    break;
                }
                break;
            }

            case BrnGameState::GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE:
                PushGuiTag(584, lpGuiInput);
                break;

            // The per-frame progress block becomes GUI 577 (one target slot per used target),
            // then the time left goes out as GUI 108.
            case BrnGameState::GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_UPDATE:
            {
                const BrnGameState::GameStateModuleIO::FreeburnChallengeUpdateAction* lpUpdateAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::FreeburnChallengeUpdateAction*>(lpAction);
                static_assert(sizeof(BrnGui::GuiChallengeUpdateEvent) == 144, "GUI 577 size 144");

                BrnGui::GuiChallengeUpdateEvent lUpdateEvent = {};
                lUpdateEvent.miCurrentAction = lpUpdateAction->miCurrentActionIndex;
                CGS_ASSERT(lUpdateEvent.miCurrentAction < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE,
                           "lUpdateEvent.miCurrentActionIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE");
                lUpdateEvent.miNumTargetsUsed = lpUpdateAction->miNumTargetsUsed;
                for (s32 liTarget = 0; liTarget < lpUpdateAction->miNumTargetsUsed; ++liTarget)
                {
                    lUpdateEvent.maiOverallTargetRemaining[liTarget] =
                        lpUpdateAction->maiOverallTargetRemaining[liTarget];
                    std::memcpy(lUpdateEvent.maafIndividualTargetContributions[liTarget],
                                lpUpdateAction->maafIndividualTargetContributions[liTarget],
                                sizeof(lUpdateEvent.maafIndividualTargetContributions[liTarget]));
                    std::memcpy(lUpdateEvent.maaeComplete[liTarget],
                                lpUpdateAction->maaeCompleted[liTarget],
                                sizeof(lUpdateEvent.maaeComplete[liTarget]));
                }
                PushGuiEvent(lUpdateEvent, lpGuiInput);

                OnlineTimeoutWire108 lTimeout;
                lTimeout.mfTimeLeft = lpUpdateAction->mfTimeLeftInChallenge;
                PushGuiEvent(lTimeout, lpGuiInput);
                break;
            }

            case BrnGameState::GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_EVERY_PLAYER_COMPLETION_STATUS:
            {
                FburnChallengeEveryPlayerStatusWire581 lEvent;
                std::memcpy(&lEvent.mStatus, lpAction, sizeof(lEvent.mStatus));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            case BrnGameState::GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_SHOW_SELECTOR:
            {
                CGS_ASSERT(lpAction != 0, "lpShowSelectorAction");
                ShowFreeburnChallengeWire582 lEvent;
                lEvent.mChallengeID =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::FburnChallengeShowSelectorAction*>(
                        lpAction)->mChallengeID;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // [RR-GUI] 279 -> GUI 338, the per-frame road-rule score. The only road-rule arm that
            // reads past the action: the crash half comes off the game module's own crash scorer
            // (the embedded ModeManager's ScoringSystem CrashModeScoring raw score and
            // multiplier), not off the action's crash score, which this arm never reads. The time
            // half is the action's, unless the debug worst-case-HUD switch is up, when its time
            // stands in. The record is a frame local on the console: the fields a cleared flag
            // skips carry stack residue there, zero here.
            case BrnGameState::GameStateModuleIO::E_ACTION_ROAD_RULES_UPDATE:
            {
                const BrnGameState::GameStateModuleIO::RoadRulesUpdateAction* lpUpdate =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::RoadRulesUpdateAction*>(lpAction);

                BrnGui::GuiEventRoadRuleUpdate lEvent;
                memset(&lEvent, 0, sizeof(lEvent));
                if (BrnGui::GuiEventRoadRuleUpdate::msbWorstCaseHudActive)
                {
                    lEvent.mfCurrentTimeElapsed = BrnGui::GuiEventRoadRuleUpdate::msfWorstCaseTimeElapsed;
                }
                else if (lpUpdate->IsScoreActive(BrnStreetData::E_SCORE_TYPE_TIME))
                {
                    lEvent.mfCurrentTimeElapsed = lpUpdate->GetScore(BrnStreetData::E_SCORE_TYPE_TIME);
                }
                if (lpUpdate->IsScoreActive(BrnStreetData::E_SCORE_TYPE_CRASH))
                {
                    const BrnGameState::CrashModeScoring* lpCrashScorer =
                        mGameStateModule.GetModeManager()->GetScoringSystem()->GetCrashScorer();
                    lEvent.miCrashMultiplier = lpCrashScorer->GetScoreMultiplier();
                    lEvent.mfCurrentCrash    = static_cast<f32>(lpCrashScorer->GetRawScore());
                }
                PushGuiEvent(lEvent, lpGuiInput);

                // [FLAG PC witness] NOT IN THE CONSOLE. Opt-in (BRN_ROADRULES_DIAG): the first 8
                // frames, then every 60th, 48 lines in all -- this action posts every frame.
                static const bool sbRoadRulesDiag      = (getenv("BRN_ROADRULES_DIAG") != 0);
                static s32        siRoadRulesDiagLeft  = 48;
                static s32        siRoadRulesDiagFrame = 0;
                ++siRoadRulesDiagFrame;
                if (sbRoadRulesDiag && siRoadRulesDiagLeft > 0 && CgsDev::Log::gpDebugPrint != 0 &&
                    (siRoadRulesDiagFrame <= 8 || (siRoadRulesDiagFrame % 60) == 0))
                {
                    --siRoadRulesDiagLeft;
                    *CgsDev::Log::gpDebugPrint
                        << "[roadrules] action 279 -> gui 338 (time " << lEvent.mfCurrentTimeElapsed
                        << " crash " << lEvent.mfCurrentCrash
                        << " x" << lEvent.miCrashMultiplier << ")\n";
                }
                break;
            }

            // ARTIST 0x823EDC74..0x823EDCA8: publish the pending developer-challenge bitset.
            case BrnGameState::GameStateModuleIO::E_ACTION_DEVELOPER_CHALLENGES_COMPLETED:
            {
                CGS_ASSERT(lpAction != 0, "lpDevCompAction");
                static_assert(sizeof(BrnGui::GuiDeveloperChallengesCompleted) == 8,
                              "ARTIST action 298 forwards one FastBitArray<15> word as GUI 596");
                BrnGui::GuiDeveloperChallengesCompleted lEvent;
                std::memcpy(&lEvent.mCompletedDeveloperChallenges, lpAction,
                            sizeof(lEvent.mCompletedDeveloperChallenges));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EDA38..0x823EDA80: action 240 publishes the 16-byte holder name
            // followed by the removal flag as raw GUI event 122 (17 bytes total).
            case BrnGameState::GameStateModuleIO::TargetEventScoreChangedResponse::KI_GAME_ACTION_TYPE:
            {
                CGS_ASSERT(lpAction != 0, "lpTargetScoreAction");
                const auto* lpResponse = reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::TargetEventScoreChangedResponse*>(lpAction);
                BrnGui::GuiEventScoreboardResponseEvScoreTarget lEvent;
                std::memcpy(lEvent.maData, &lpResponse->mPlayerName, sizeof(lpResponse->mPlayerName));
                lEvent.maData[16] = static_cast<u8>(lpResponse->mbRemoved);
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EDA84..0x823EDAC4: action 241's 16 raw name bytes are copied over
            // the full GUI-event-123 object before AddGuiEvent queues all 16 bytes.
            case BrnGameState::GameStateModuleIO::DldScoreboardResponse::KI_GAME_ACTION_TYPE:
            {
                CGS_ASSERT(lpAction != 0, "lpDldChallengeableAction");
                const auto* lpResponse = reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::DldScoreboardResponse*>(lpAction);
                BrnGui::GuiEventScoreboardDownloadedChallengeable lEvent;
                static_assert(sizeof(lEvent) == sizeof(*lpResponse),
                              "ARTIST case 241 copies the same 16-byte record to GUI event 123");
                std::memcpy(&lEvent, lpResponse, sizeof(lEvent));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EC84C..0x823EC89C: action 288 answers an event-data request with
            // {event id, score, player name[16]}. The GUI-332 consumer reads those bytes at
            // +0/+8/+0xC; the final four bytes of this 32-byte local are never written.
            case BrnGameState::GameStateModuleIO::TargetEventDataResponse::KI_GAME_ACTION_TYPE:
            {
                CGS_ASSERT(lpAction != 0, "lpChallengeEventDataAction");
                const auto* lpResponse = reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::TargetEventDataResponse*>(lpAction);
                BrnGui::GuiEventChallengedEventDataResponse lEvent;
                std::memcpy(&lEvent, &lpResponse->mEventId, sizeof(lpResponse->mEventId));
                lEvent.muHeader2 = static_cast<u32>(lpResponse->miScore);
                std::memcpy(lEvent.maPayload, &lpResponse->mPlayerName, sizeof(lpResponse->mPlayerName));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            default:
                // [stuntrace wave E1, 2026-08-26] the EVENT-FLOW arms (23/37/38/39/44/47/200/201)
                // live in the sibling GameBridgeGameStateToX_EventFlowGuiEvents.cpp -- one drain
                // walk, split across sibling TUs exactly like this one.
                // [FLAG] original explicit action cases without an arm remain unresolved; their absence here is not a no-op disposition.
                BrnGame::TranslateEventFlowGameActionToGuiEvent(
                    liActionType, lpAction, lpGuiInput, lpGameStateOutput);
                break;
            }

            const CgsModule::Event* lpNextAction = 0;
            liActionType = lpActionQueue->GetNextEvent(lpAction, &lpNextAction, &liActionSize);
            lpAction     = lpNextAction;
        }

        // ---- the tail: the pending sat-nav record, posted once ------------------------------
        // The console posts the pending GuiEventUpdateSatNav (id 199, 2320 bytes) after the drain
        // when its icon count is > 0. (It follows the console's unconditional
        // GuiEventRacePositionInfo post above; the two are
        // independent.) The count is written into the record's own +0x900 word, which is what
        // the cache's case-199 arm reads.
        // ARTIST0x823EDCE0..0x823EDD1C: publish standings even on frames with no actions.
        const auto* scoring = lpGameStateOutput->GetScoringOutputInterface();
        BrnGui::GuiEventRacePositionInfo positions;
        for (s32 i = 0; i < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++i)
        {
            positions.maiPositions[i] = static_cast<s8>(scoring->maCarScoreData[i].GetRacePosition());
            positions.mabFinished[i] = scoring->maCarScoreData[i].GetHasFinished();
            positions.mabValid[i] = scoring->mabValid[i];
        }
        PushGuiEvent(positions, lpGuiInput);

        if (liPendingSatNavIcons > 0)
        {
            lPendingSatNavEvent.miNumIcons = liPendingSatNavIcons;
            PushGuiEvent(lPendingSatNavEvent, lpGuiInput);

            // [DIAG] NOT IN THE X360 BINARY -- one line per publish (boot / discovery / junkyard
            // exit), same `[drivethru]` tag as the producer's SETUP line.
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[drivethru] BRIDGE: action 45 -> gui 199 with " << liPendingSatNavIcons
                    << " drive-thru icon record(s)\n";
            }
        }
    }
} // namespace BrnGame
