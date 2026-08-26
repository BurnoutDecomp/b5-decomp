// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX.cpp
//
// The BrnGame::BrnGameModule game-state->X bridge family. Reconstructed store-for-store
// from BURNOUT_X360_ARTIST.XEX. Only the verified functions of this TU are homed here;
// the two top-level bridge entry points (BridgeGameStateToGui 0x823EE880 /
// BridgeGameStateToNetwork 0x823E2398) remain DEFERRED (blocked: their pseudocode is not
// store-for-store faithful -- ToGui's IDA output is flagged "local variable allocation has
// failed" over ~160 locals with un-homed GUI event payloads + VariableEventQueue Append;
// ToNetwork is a ~240-case switch over un-homed network-action payloads) and are homed by
// their owning batches once those event layouts land.
//
//   BridgeGameStateToController     0x823C0AE8  [reconstructed]
//   ConvertTrainingTypeToStringId   0x823AA3B8  [MOVED 2026-08-16 ->
//                                    GameBridgeGameStateToX_TrainingStringIds.cpp; see the
//                                    MOVED-OUT block below for the four compile errors that
//                                    keep THIS file out of the build]
//   TranslateTakedownsToGuiEvents   0x823E1C38  [reconstructed; COMPILES as of 2026-08-20 --
//                                    see the ✅ block below; its SOFT arm is parked on a
//                                    GuiSoftTakedownEvent type grow]
//   MapStuntEnumsFromGameplayToGui  0x823AA4A8  [MOVED 2026-08-20 ->
//                                    GameBridgeGameStateToX_StuntGuiEvents.cpp]
//   TranslateGameActionsToGuiEvents 0x823E9CE0  [PARTIAL (cases 58/59/60), same sibling TU]
//
// ⛔ THIS TU COMPILES BUT IS STILL NOT MOUNTABLE, and the reason has CHANGED: it is no
// longer the four compile errors (fixed), it is SIX UNRESOLVED EXTERNALS with no definition
// anywhere in b5-decomp/src -- all of them reached BY NAME from the two bodies that remain
// here, both of which are callerless today:
//     BrnGameState::GetTakedownEventOutputCount        (TranslateTakedownsToGuiEvents)
//     BrnGameState::GetTakedownEventOutputRecord       (        "                    )
//     BrnGameState::GetGameStateInputBindRequestQueue  (BridgeGameStateToController)
//     BrnGameState::GetGameStateInputUnbindRequestQueue(        "                  )
//     CgsInput::InputIO::PostWorldInputBuffer::PostBindRequest    (        "       )
//     CgsInput::InputIO::PostWorldInputBuffer::PostUnbindRequest  (        "       )
// (X360 sub_823B9CD8 is the bind-request-queue accessor; the unbind queue sits at +0x4C from
// it. Their canonical home is the game-state IO TU.) That is why the two NEW bodies this wave
// added were split into GameBridgeGameStateToX_StuntGuiEvents.cpp -- they ARE called every
// sub-step by BrnGameModule::Update, so they have to be linkable now. Home those six and this
// whole TU (plus the ~700-case translate switch) can be mounted and the split folded back.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGameStateToX.h"
#include "GameSource/Game/GameBridgeControllerToX.h"                // CgsGui::GuiModule + AddGuiEvent<T> (established placeholder home)

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"  // CgsInput::InputIO::PostWorldInputBuffer / BaseInputEvent
#include "SharedClasses/Progression/BrnTrainingTypes.h"            // BrnProgression::ETrainingType
// [gateui] the real GUI-event payload homes (the two placeholders this TU used to fork live
// here) + the game-action payload homes the stunt arms read.
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                    // BrnGui::GuiTakedownEvent / StuntType / GuiEventStunt*
// ⛔ [gateui] GameSource/Gui/BrnGuiDemangledEventTypes.h is DELIBERATELY NOT INCLUDED HERE, and
// that is a MEASURED blocker, not a preference. It re-defines two payload types that are ALSO
// forked in headers this TU already reaches through BrnGameModule.hpp:
//     BrnGuiDemangledEventTypes.h:122 GuiEventNetworkPlayerImage
//         vs GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.h:75
//     BrnGuiDemangledEventTypes.h:196 GuiEventToggleChangeCarMessage
//         vs GameSource/Game/GameBridgeControllerToX.h:113
// -- two more C2011s of exactly the class this wave just removed from
// GameBridgeGameStateToX.h. Both are pre-existing and neither is repaired here: deleting the
// GameBridgeControllerToX.h fork (this lane's half) would force ITS TU to include the
// demangled header, where the renderer fork -- a Gui-lane file -- would break it in turn. Filed
// as a shared_header_request; the consequence for this TU is the parked autosave-request post
// in the action-58 arm.
#include "GameSource/GameState/BrnGameActions.h"                   // BrnGameState::GameStateModuleIO action payloads
#include "GameSource/GameState/BrnGameStateModuleIO.h"             // GameStateModuleIO::OutputBuffer / GameActionQueue
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"             // CgsGuiModuleIO::InputBuffer::GetGuiEvents()
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"          // Io::RootInputBuffer (BridgeGameStateToSound; phase C3b)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // CgsDev::Log::gpDebugPrint ([UI-gate] diag)
#include <stdlib.h>                                                // getenv (the [UI-gate] diag guard)

namespace BrnGame
{
    // =========================================================================
    // BridgeGameStateToController  (X360 0x823C0AE8)
    // Merge the game-state output's input-bind / input-unbind REQUEST queues into the input
    // module's post-world input buffer: PostBindRequest per queued bind, PostUnbindRequest per
    // queued unbind. When either queue produced any work, stamp miInputModuleState (@ +10094136)
    // with the bind (3) / unbind (6) sentinel. Called by DoUpdate_InputPostWorld.
    //
    // FLAG: the game-state input bind/unbind request-queue accessors (X360 sub_823B9CD8; the
    // second queue sits at +0x4C from the first) are un-homed and reached by name with the
    // committed CgsInput bind/unbind request-queue types.
    // =========================================================================
    void BrnGameModule::BridgeGameStateToController(
        BrnGameState::GameStateModule* lpGameStateOutput,
        CgsInput::InputIO::PostWorldInputBuffer* lpPostWorldInput)
    {
        const CgsInput::InputIO::PostWorldInputBuffer::BindRequestQueue* lpBindQueue =
            BrnGameState::GetGameStateInputBindRequestQueue(lpGameStateOutput);
        const CgsInput::InputIO::PostWorldInputBuffer::UnBindRequestQueue* lpUnbindQueue =
            BrnGameState::GetGameStateInputUnbindRequestQueue(lpGameStateOutput);

        CGS_ASSERT(lpBindQueue   != 0, "lpGameStateInputBindRequestQueue");
        CGS_ASSERT(lpUnbindQueue != 0, "lpGameStateInputUnbindRequestQueue");

        // Bind requests: element i carries {word0, word1} (asm ld 8 bytes then split r4/r5).
        const s32 liNumBind = lpBindQueue->GetLength();   // *(queue+8)
        for (s32 i = 0; i < liNumBind; ++i)
        {
            const CgsInput::InputIO::BaseInputEvent& lrEvent = lpBindQueue->GetEvent(i);
            const s32* lpWords = reinterpret_cast<const s32*>(&lrEvent);
            lpPostWorldInput->PostBindRequest(lpWords[0], lpWords[1]);
        }
        if (liNumBind > 0)
            miInputModuleState = 3;

        // Unbind requests: element j carries a single request word (asm *v12 -> PostUnbindRequest).
        const s32 liNumUnbind = lpUnbindQueue->GetLength();   // *(queue+8)
        for (s32 j = 0; j < liNumUnbind; ++j)
        {
            const CgsInput::InputIO::BaseInputEvent& lrEvent = lpUnbindQueue->GetEvent(j);
            const s32* lpWords = reinterpret_cast<const s32*>(&lrEvent);
            lpPostWorldInput->PostUnbindRequest(lpWords[0]);
        }
        if (liNumUnbind > 0)
            miInputModuleState = 6;
    }


    // ------------------------------------------------------------------------
    // MOVED OUT (2026-08-16, tutorial-ticker leg):
    //   ConvertTrainingTypeToStringId @0x823AA3B8 + its two rodata string-ID tables now
    //   live in the per-function sibling TU
    //   GameSource/Game/GameBridgeGameStateToX_TrainingStringIds.cpp.
    //   MOVED, not copied -- folding it back in later is a delete, not a duplicate-symbol hunt
    //   (the same _Prepare.cpp / _SetupParRivals.cpp precedent the build script documents).
    //
    // ✅ [gateui] 2026-08-20: THE FOUR ERRORS BELOW ARE FIXED AND THIS TU COMPILES AGAIN.
    // They were ONE defect with three symptoms -- the header forked two GUI event payload
    // types that have real homes -- plus one stale member name:
    //   1/2/4. The `BrnGui::GuiTakedownEvent` / `BrnGui::GuiSoftTakedownEvent` placeholders are
    //          deleted from GameBridgeGameStateToX.h; the real homes are included instead
    //          (BrnGuiEventTypeDefs.h / BrnGuiDemangledEventTypes.h). The two consequent
    //          C2027s fall out with them.
    //   3.     `mpCgsGuiModule` is gone from the body: the GUI events are pushed onto the
    //          input buffer's own queue by PushGuiEvent (GameBridgeGameStateToX.h) -- which is
    //          what the console's AddGuiEvent<T> instantiation does anyway, and needs no
    //          module object.
    // The MOVED-OUT split of ConvertTrainingTypeToStringId is LEFT AS IT IS (moving it back is
    // a delete, not a duplicate-symbol hunt -- do it in a commit of its own if wanted).
    // The historical diagnosis is preserved verbatim below.
    //
    // ⛔ WHY THE SPLIT: **THIS TU DID NOT COMPILE**, and did not before this leg either --
    // MEASURED with a control (HEAD's own copy of this file, compiled with the canonical
    // build flags, produces the SAME four errors, so none of them is new work):
    //   1. C2011 'BrnGui::GuiTakedownEvent': struct type redefinition. The placeholder in
    //      GameBridgeGameStateToX.h collides with the real 40-byte record in
    //      BrnGuiEventTypeDefs.h:1056. ⚠️ That header's own comment claims "the include
    //      graphs do not meet" -- THEY DO. This TU includes BrnGameModule.hpp, which reaches
    //      BrnGuiEventTypeDefs.h. The claim is stale and the compiler falsifies it.
    //   2. The same fork exists for GuiSoftTakedownEvent (the bridge header's field-by-field
    //      placeholder vs BrnGuiDemangledEventTypes.h:264's opaque GuiEvent<364>+u8[20]);
    //      the compiler only reports #1 because it stops at the first definition clash.
    //   3. C2065 'mpCgsGuiModule': undeclared identifier. BrnGameModule no longer has that
    //      member -- it holds mGuiModule (a BrnGui::GuiModule BY VALUE, hpp:620). This TU is
    //      stale against a rename.
    //   4. The two C2027s are consequences of 1+3.
    // All four are inside TranslateTakedownsToGuiEvents / its header block. Repairing them is
    // a takedown-HUD job (two placeholder records to reconcile against two different canonical
    // types, one of them itself opaque) with no way to verify from here -- so it is NAMED, not
    // guessed at. ConvertTrainingTypeToStringId shares none of that surface, so it is split out
    // and mounted on its own.
    // ------------------------------------------------------------------------

    // =========================================================================
    // TranslateTakedownsToGuiEvents  (X360 0x823E1C38)
    // For each record in the game-state output's TakedownEvent output queue, synthesise a takedown
    // GUI event and push it through the CgsGui GUI module (this + 7252512). A record is a HARD
    // takedown (BrnGui::GuiTakedownEvent) when its race-car index equals the runner index, or when
    // the soft-takedown-display flag bit is clear; otherwise it is a SOFT takedown
    // (BrnGui::GuiSoftTakedownEvent) which carries only the leading fields + two status bytes.
    //
    // FLAG: the takedown queue element accessor (BrnGameState::GetTakedownEventOutputRecord), the
    // GUI event payload layouts, and the soft-display flag word are un-homed placeholders reached
    // by name (see GameBridgeGameStateToX.h / BrnGameModule.hpp) -- the parity contract encoded here
    // is the record-by-record hard/soft classification + store-for-store field copy.
    // =========================================================================
    void BrnGameModule::TranslateTakedownsToGuiEvents(
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
        const void* lpTakedownQueue,
        s32 liRunnerActiveRaceCarIndex)
    {
        // X360 `li r,1; extldi r,r,64,33` -> 64-bit mask 0x0000000200000000 (bit 33).
        static const u64 KU_TAKEDOWN_SOFT_DISPLAY_MASK = 0x0000000200000000ull;

        const s32 liCount = BrnGameState::GetTakedownEventOutputCount(lpTakedownQueue);  // *(queue+8)
        for (s32 i = 0; i < liCount; ++i)
        {
            const BrnGameState::TakedownEventOutputRecord* lpRecord =
                BrnGameState::GetTakedownEventOutputRecord(lpTakedownQueue, i);

            // Hard when the record's race-car index matches the runner, or the soft-display bit is clear.
            bool lbHard;
            if (lpRecord->miRaceCarIndex == liRunnerActiveRaceCarIndex)
            {
                lbHard = true;
            }
            else
            {
                lbHard = ((mu64TakedownDisplayFlags & KU_TAKEDOWN_SOFT_DISPLAY_MASK) == 0);
            }

            if (lbHard)
            {
                // The HARD arm, store for store (@0x823E1D24..0x823E1D74). Field names are the
                // real ones from BrnGuiEventTypeDefs.h; the offsets they land on are exactly
                // the console's -- see that header's own producer/consumer store map.
                BrnGui::GuiTakedownEvent lEvent;
                lEvent.mAggressorCarID        = lpRecord->mu64Field08;   // +0x00 <- src +0x08
                lEvent.mVictimCarID           = lpRecord->mu64Field10;   // +0x08 <- src +0x10
                lEvent.meAggressorIndex       =
                    static_cast< ::EActiveRaceCarIndex>(lpRecord->miField00);       // +0x10 <- src +0x00
                lEvent.meVictimIndex          =
                    static_cast< ::EActiveRaceCarIndex>(lpRecord->miRaceCarIndex);  // +0x14 <- src +0x04
                lEvent.meTakedownType         =
                    static_cast<BrnGameState::ETakedownType>(lpRecord->miField18);         // +0x18 <- src +0x18
                lEvent.miTakedownChainCount   = lpRecord->miField20;     // +0x1C <- src +0x20
                lEvent.miMultipleTakedownCount= lpRecord->miField1C;     // +0x20 <- src +0x1C
                lEvent.mbMarkedManTakeDown    = (lpRecord->mbField24 != 0);  // +0x24 <- src +0x24
                lEvent.mbSettledScore         = (lpRecord->mbField26 != 0);  // +0x25 <- src +0x26
                PushGuiEvent(lEvent, lpGuiInput);                       // AddEvent(&ev, 363, 40)
            }
            else
            {
                // ⛔ [gateui] PARKED, NOT DROPPED -- THE SOFT ARM NEEDS A TYPE GROW I MAY NOT MAKE.
                // The console's soft record (@0x823E1CDC..0x823E1D1C) is 32 bytes of PLAIN data,
                // decoded store for store off the same 40-byte source image the hard arm uses:
                //     +0x00 CgsID               <- src +0x08   (std)
                //     +0x08 CgsID               <- src +0x10   (std)
                //     +0x10 EActiveRaceCarIndex <- src +0x00   (stw)
                //     +0x14 EActiveRaceCarIndex <- src +0x04   (stw)
                //     +0x18 ETakedownType       <- src +0x18   (stw)
                //     +0x1C bool                <- src +0x24   (stb)
                //     +0x1D bool                <- src +0x26   (stb)
                // i.e. GuiTakedownEvent's leading five fields plus the two status bytes pulled
                // forward to +0x1C/+0x1D (the soft variant carries no chain/multiple counts).
                // The committed home, BrnGuiDemangledEventTypes.h:271, is the auto-derived
                // honest placeholder `GuiSoftTakedownEvent : public CgsGui::GuiEvent<364>
                // { u8 maPayload[20]; }` -- the right SIZE (32, matching AddGuiEvent<...>
                // @0x823D9AD0's `AddEvent(q, ev, 364, 32)`) but the WRONG SHAPE: the asm above
                // proves the record has no 12-byte GuiEvent header, its first store is at +0x00.
                // Filling that opaque payload by byte offset from here is exactly the
                // offset-poke this project forbids, and re-forking the type is what broke this
                // TU in the first place.
                // SHARED_HEADER_REQUEST (owner: the Gui lane) -- grow GuiSoftTakedownEvent in
                // GameSource/Gui/BrnGuiEventTypeDefs.h to the seven fields above, next to its
                // GuiTakedownEvent sibling (static_assert sizeof == 32), and retire the
                // demangled placeholder. This arm is then five lines, spelled out above.
                // ⓘ SCOPE: no regression. This TU has never compiled or been mounted, so the
                // soft-takedown HUD event has never been posted on PC. Landing the hard arm is
                // a strict improvement; this park is the honest remainder.
                (void)lpRecord;
            }
        }
    }

    // @ 0x823CDE50 (bodied 2026-08-25, faithful-audio-engine phase C3b). The
    // game-state -> sound input bridge (the caller holds the game-state output's
    // read lock + the sound input's write lock):
    //   [1] the 13312 game-action queue append (both ends are the same
    //       VariableEventQueue<13312,16> instantiation -- the console append
    //       symbol pins them; the root side's GetGameActionQueue write accessor).
    //   [2] the three interface installs -- game-mode (+176344), scoring
    //       (+173240), online-scoring (+175976), each opaque view cast onto its
    //       root-side twin (same console record, spans match).
    //   [3] the UpdateInfo byte: the two +1009411x game-module flags OR'd (their
    //       writers are un-decoded -- both stay false, publishing 0, the
    //       boot-state value).
    void BrnGameModule::BridgeGameStateToSound(
        BrnSound::Module::Io::RootInputBuffer* lpSoundInputBuffer,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutputBuffer)
    {
        typedef BrnSound::Module::Io::RootInputBuffer RootIn;

        lpSoundInputBuffer->GetGameActionQueue().Append(
            *lpGameStateOutputBuffer->GetGameActionQueue());

        lpSoundInputBuffer->SetGameModeInterface(
            reinterpret_cast<const RootIn::GameModeOutputInterface*>(
                lpGameStateOutputBuffer->GetGameModeOutputInterface()));
        lpSoundInputBuffer->SetScoringInterface(
            reinterpret_cast<const RootIn::ScoringOutputInterface*>(
                lpGameStateOutputBuffer->GetScoringOutputInterface()));
        lpSoundInputBuffer->SetOnlineScoringInterface(
            reinterpret_cast<const RootIn::OnlineScoringOutputInterface*>(
                lpGameStateOutputBuffer->GetOnlineScoringOutputInterface()));

        RootIn::UpdateInfo lUpdateInfo;
        lUpdateInfo.mData[0] = (mbField10094120 || mbField10094119) ? 1 : 0;
        lpSoundInputBuffer->SetUpdateInfo(&lUpdateInfo);
    }

} // namespace BrnGame
