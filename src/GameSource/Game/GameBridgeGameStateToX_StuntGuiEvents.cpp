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
    // ~700-case jump table (`jpt_823EA1F0`) over every game action in the build. FOUR arms are
    // reproduced here -- 58 / 59 / 60, the stunt-collectible family, plus 148, the training
    // ticker ([tut-ticker] 2026-08-24) -- and
    // every other action falls through the default with NO event posted. That is a real
    // behavioural gap for the other ~700 actions, not a silent one: each of them is currently
    // unreachable anyway (this TU has never been mounted, and BridgeGameStateToGui @0x823EE880,
    // the only caller, is still DEFERRED), so nothing regresses; but a future owner adding, say,
    // the takedown or road-rules arms must add them HERE rather than in a parallel function.
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
