// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX_StuntGuiEvents.cpp
//
// ⭐⭐ [gateui] THE GAME-STATE -> GUI STUNT-COLLECTIBLE ARMS. Two BrnGame::BrnGameModule
// members whose DWARF home is GameSource/Game/GameBridgeGameStateToX.cpp:
//
//   MapStuntEnumsFromGameplayToGui   0x823AA4A8   (a real X360 symbol, whole)
//   TranslateGameActionsToGuiEvents  0x823E9CE0   (PARTIAL -- cases 58/59/60 only)
//
// ⛔ WHY A SIBLING TU RATHER THAN THE PARENT FILE. GameBridgeGameStateToX.cpp COMPILES
// again as of this wave (the two forked GUI-event placeholders that produced its four
// C2011/C2065 errors are gone), but it still cannot be MOUNTED: its other two bodies
// reach six by-name symbols that have no definition anywhere in b5-decomp/src --
//     BrnGameState::GetTakedownEventOutputCount / GetTakedownEventOutputRecord
//     BrnGameState::GetGameStateInputBindRequestQueue / GetGameStateInputUnbindRequestQueue
//     CgsInput::InputIO::PostWorldInputBuffer::PostBindRequest / PostUnbindRequest
// -- i.e. mounting it is six LNK2019s. Both of those bodies are also CALLERLESS today
// (BridgeGameStateToGui @0x823EE880 and DoUpdate_InputPostWorld are unreconstructed),
// whereas the two functions HERE are called every sub-step by BrnGameModule::Update, so
// they must be linkable NOW. Split, exactly as ConvertTrainingTypeToStringId was split into
// GameBridgeGameStateToX_TrainingStringIds.cpp on 2026-08-16 -- MOVED, not copied, so
// folding it back in once those six symbols are homed is a delete, not a
// duplicate-symbol hunt.
//
// The shared GUI-event push (PushGuiEvent) lives in GameBridgeGameStateToX.h so both TUs
// use one copy; its banner explains why the queue is written directly instead of through
// CgsGui::GuiModule::AddGuiEvent<T>.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGameStateToX.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                    // BrnGui::StuntType / GuiEventStunt*
#include "GameSource/Gui/Events/BrnGuiEventRankProgressResponse.h" // GuiEventRankProgressResponse (case 181)
#include "GameSource/Gui/Events/BrnGuiEventStatsResponse.h"        // GuiEventStatsResponse (case 180)
#include "GameSource/GameState/SharedIO/BrnGameActionData.h"        // GameStateModuleIO::GameStats (the case-180 record)
#include "GameSource/GameState/Progression/BrnProgressionManager.h" // ProgressionManager / Profile (case 180)
#include "SharedClasses/Trigger/BrnGenericRegion.h"                  // GenericRegion::Type (the drive-thru set query)
#include "GameSource/GameState/BrnGameActions.h"                   // the action payload homes
#include "GameSource/GameState/BrnGameStateModuleIO.h"             // OutputBuffer / GameActionQueue
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"             // InputBuffer::GetGuiEvents()
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // CgsDev::Log::gpDebugPrint
#include <stdlib.h>                                                // getenv (the [UI-gate] diag guard)
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

    // =========================================================================
    // [profile-save] the on-queue record of GUI event 356 (BrnGui::GuiAutosaveRequestEvent),
    // TU-LOCAL for exactly the reason the 537 record above is: this TU cannot include
    // BrnGuiDemangledEventTypes.h (the C2011 fork pair documented in the include banner).
    // The identical TU-local record already exists in the sibling
    // GameBridgeGameStateToX_EventFlowGuiEvents.cpp (AutosaveRequestWire356) for its case-37
    // post; both are file-static in an anonymous namespace, so there is no ODR fork -- the
    // canonical type stays BrnGuiDemangledEventTypes.h:56.
    // Wire shape: AddGuiEvent<GuiAutosaveRequestEvent> @0x823D03E0 posts ONE byte (id 356,
    // size 1) -- the console builds it with a single `stb` and never a GuiEvent header.
    // =========================================================================
    struct AutosaveRequestWire356
    {
        u8 mu8Flag;                             // +0x00 (the console's `stb` byte)
        s32 GetEventType() const { return 356; }
    };
    static_assert(sizeof(AutosaveRequestWire356) == 1, "id 356 size 1");

    // =========================================================================
    // [drive-thru wave 2026-08-29] The byte the BODY-SHOP drive-thru action (97) carries its
    // "the repair did something" flag in. The 144-byte shop payload is a serialised action
    // blob, not a C++ class: transform @+0x00, an identity block @+0x40, per-action scalars
    // from +0x80. For action 97 the producer (DriveThruManager::PostShopAction) writes the
    // player entity id at +0x80, the not-online flag at +0x84 and this byte at +0x85, and the
    // console's bridge arm reads exactly it back (`lbz r11, 0x85(r31)` @0x823EB5C8).
    // =========================================================================
    const s32 KI_SHOP_ACTION_EFFECTIVE_BYTE = 0x85;
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
    // ⚠️⚠️ PARTIAL RECONSTRUCTION, AND IT IS NAMED, NOT HIDDEN. The console body is a
    // ~700-case jump table (`jpt_823EA1F0`) over every game action in the build. The arms
    // reproduced here are 58 / 59 / 60 (the stunt-collectible family), 55, 112, 148 (the
    // training ticker, [tut-ticker] 2026-08-24), 181, and 97 / 98 / 100 / 101 (the drive-thru
    // family, [drive-thru] 2026-08-29); the event-flow arms live in the sibling
    // GameBridgeGameStateToX_EventFlowGuiEvents.cpp, reached through the `default:` below.
    // Every other action falls through with NO event posted. A future owner adding, say, the
    // takedown or road-rules arms must add them HERE rather than in a parallel function.
    //
    // ⛔⛔ THE SENTENCE THAT STOOD HERE WAS STALE, AND IT WAS THE LOAD-BEARING ONE. It read:
    // "each of them is currently unreachable anyway (this TU has never been mounted, and
    //  BridgeGameStateToGui @0x823EE880, the only caller, is still DEFERRED), so nothing
    //  regresses". BOTH HALVES ARE FALSE and have been for a while: this TU is mounted
    // (tools/build/build_game_exe.bat), and TranslateGameActionsToGuiEvents is called EVERY
    // FRAME from BrnGameModule.cpp inside the LockForRead/LockForWrite bracket, because the PC
    // build re-seats the console's BridgeGameStateToGui call sequence inline. So a missing arm
    // here is a LIVE behavioural gap, not a parked one -- which is exactly what it turned out
    // to be for the drive-thru: the mechanic worked and the game said nothing for a day because
    // 97/98/100 were absent from a drain loop that was running the whole time. Do not write
    // "unreachable anyway" into this banner again without re-checking both facts.
    // The nearest sibling arms, for whoever comes next: 55 -> GuiAutosaveRequestEvent(356);
    // 57 -> GuiEventJumpStarted(216) @0x823EB868; 61 has NO case (the boost action is consumed
    // off the HUD path); 127 -> the Showtime-only CrashModeScoring leg, mode-gated 2/16.
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

        const CgsModule::Event* lpAction     = 0;
        s32                     liActionSize = 0;
        s32                     liActionType = lpActionQueue->GetFirstEvent(&lpAction, &liActionSize);

        while (lpAction != 0)
        {
            switch (liActionType)
            {
            // ---- 58  E_ACTION_ON_STUNT_ELEMENT_COMPLETE (24 bytes) ------------------------
            // ⭐⭐ THE BILLBOARD / SMASH-GATE HUD POPUP. @0x823EB870..0x823EB96C.
            //   lwz r11, 0x14(r31)   -- meCurrentGameMode picks the presentation
            //   cmplwi r11, 0x11 ; bgt default            (UNSIGNED: E_MODE_NONE == -1 -> default)
            //   jump table jpt_823EB894: cases 0-8,10-14,16,17 -> the BOOST-BAR arm,
            //                            cases 9,15 (+ anything > 17) -> the plain arm
            //   both arms: { miCurrentCount@+0x00 <- action+0x0C,
            //                miTotalCount  @+0x04 <- action+0x10,
            //                meStuntType   @+0x08 <- MapStuntEnumsFromGameplayToGui(action+0x08) }
            //   both arms then post a 1-byte GuiAutosaveRequestEvent (356) built by
            //   `stb r19` with r19 == 0 (@0x823EB874 / @0x823EB920 / @0x823EB968).
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

                // ⛔ [gateui] PARKED: the console builds a 1-byte GuiAutosaveRequestEvent ONCE
                // at the head of this case (`stb r19, var_35D8` @0x823EB874, r19 == 0) and
                // posts it from BOTH arms (@0x823EB920 / @0x823EB968), id 356 size 1
                // (AddGuiEvent<GuiAutosaveRequestEvent> @0x823D03E0). Its only home is
                // BrnGuiDemangledEventTypes.h:47, and this TU cannot include that header --
                // see the C2011 fork pair documented at the include block above. Re-forking
                // the type here is what broke this TU in the first place, so the post is
                // dropped and named rather than faked.
                // CONSEQUENCE: collecting a billboard/smash gate will not request a profile
                // autosave. The HUD popup itself (the wave's proof) is unaffected.
                // SHARED_HEADER_REQUEST (owner: the Gui lane) -- remove the
                // GuiEventNetworkPlayerImage fork from
                // GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.h:75.
                // Once that is gone this TU can include the demangled header and both arms
                // become one extra `PushGuiEvent(lAutosaveRequest, lpGuiInput);`.

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
                }
                else
                {
                    BrnGui::GuiEventStuntInfo lEvent;                   // id 217, 12 bytes
                    lEvent.miCurrentCount = lpStunt->miCurrentCount;
                    lEvent.miTotalCount   = lpStunt->miTotalCount;
                    lEvent.meStuntType    = leGuiStuntType;
                    liPostedGuiEventId    = lEvent.GetEventType();
                    PushGuiEvent(lEvent, lpGuiInput);
                }
                // (the parked GuiAutosaveRequestEvent post would go here -- see above)

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
                AutosaveRequestWire356 lEvent;
                lEvent.mu8Flag = *reinterpret_cast<const u8*>(lpAction);   // X360 `HIBYTE(v282) = *v7`
                PushGuiEvent(lEvent, lpGuiInput);

                if ( sbPropDiag && siDiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
                {
                    --siDiagLinesLeft;
                    *CgsDev::Log::gpDebugPrint
                        << "[profile-save] action 55 -> gui 356 (flag "
                        << static_cast<s32>(lEvent.mu8Flag) << ")\n";
                }
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
            // ProgressionManager::miSponsorCarCount (+133468, the second halved with
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
                lEvent.miCarsTotal  = lpProgressionManager->GetSponsorCarCount();
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
                if (liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
                    liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
                {
                    TranslateShowtimeActionToGuiEvent(liActionType, lpAction, lpGuiInput);
                }
                break;
            }

            default:
                // [stuntrace wave E1, 2026-08-26] the EVENT-FLOW arms (23/37/38/39/44/47/200/201)
                // live in the sibling GameBridgeGameStateToX_EventFlowGuiEvents.cpp -- one drain
                // walk, split across sibling TUs exactly like this one.
                // [FLAG] the remaining ~690 console arms are not reproduced -- see the banner.
                BrnGame::TranslateEventFlowGameActionToGuiEvent(
                    liActionType, lpAction, lpGuiInput, lpGameStateOutput);
                break;
            }

            const CgsModule::Event* lpNextAction = 0;
            liActionType = lpActionQueue->GetNextEvent(lpAction, &lpNextAction, &liActionSize);
            lpAction     = lpNextAction;
        }
    }
} // namespace BrnGame
