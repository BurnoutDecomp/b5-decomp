#pragma once
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX.h
//
// Public declarations for the GameState->X bridge TU (DWARF home
// GameSource/Game/GameBridgeGameStateToX.cpp). Only the surface needed by the
// reconstructed function(s) in this batch is declared here; other bridge entry
// points are added by their owning batches.
// ============================================================================

#include "SharedClasses/Progression/BrnTrainingTypes.h"   // BrnProgression::ETrainingType
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h" // CgsInput::InputIO::PostWorldInputBuffer

namespace BrnGameState { class GameStateModule; }

namespace BrnGameState
{
    // Un-homed game-state accessors reached by name from
    // BrnGameModule::BridgeGameStateToController (X360 0x823C0AE8: sub_823B9CD8(a3) returns the
    // bind-request queue; the unbind-request queue sits at +0x4C from it). Their canonical home
    // is the game-state IO; declared here so the bridge TU resolves them until that TU lands.
    const CgsInput::InputIO::PostWorldInputBuffer::BindRequestQueue*
        GetGameStateInputBindRequestQueue(GameStateModule* lpGameStateOutput);
    const CgsInput::InputIO::PostWorldInputBuffer::UnBindRequestQueue*
        GetGameStateInputUnbindRequestQueue(GameStateModule* lpGameStateOutput);

    // ---- takedown-event output-queue element + accessors ---------------------------------------
    // TranslateTakedownsToGuiEvents (X360 0x823E1C38) walks the OutputBuffer's TakedownEventOutput
    // queue (BrnGameState::GameStateModuleIO::TakedownEventOutputQueueType @ +0x4040), reading the
    // element count at +8 and copying each 40-byte record out via BrnGameState::TakedownEvent__.
    // Both are un-homed here (their canonical home is the game-state IO TU) and reached BY NAME; the
    // queue is passed opaquely (X360 treats it as a raw _DWORD*). FLAG: the 40-byte record's field
    // semantics beyond the compared race-car index are not attested -- modelled store-for-store from
    // the X360 copy (5 qwords) + the fields the bridge reads.
    struct TakedownEventOutputRecord
    {
        s32 miField00;        // +0x00 (X360 v23 -> GuiEvent +0x10)
        s32 miRaceCarIndex;   // +0x04 (X360 v24 -> GuiEvent +0x14; compared against the runner index)
        u64 mu64Field08;      // +0x08 (X360 v25 -> GuiEvent +0x00)
        u64 mu64Field10;      // +0x10 (X360 v26 -> GuiEvent +0x08)
        s32 miField18;        // +0x18 (X360 v27 -> GuiEvent +0x18)
        s32 miField1C;        // +0x1C (X360 v28 -> hard GuiEvent +0x20)
        s32 miField20;        // +0x20 (X360 v29 -> hard GuiEvent +0x1C)
        u8  mbField24;        // +0x24 (X360 v30 -> GuiEvent status byte 0)
        u8  mbPad25;          // +0x25
        u8  mbField26;        // +0x26 (X360 v31 -> GuiEvent status byte 1)
        u8  mbPad27;          // +0x27
    };

    s32 GetTakedownEventOutputCount(const void* lpTakedownQueue);
    const TakedownEventOutputRecord* GetTakedownEventOutputRecord(
        const void* lpTakedownQueue, s32 liIndex);
}

// ⛔ [gateui] THE TWO GUI-EVENT PLACEHOLDERS THAT LIVED HERE ARE GONE (2026-08-20).
// This header used to define file-local `BrnGui::GuiTakedownEvent` and
// `BrnGui::GuiSoftTakedownEvent`, justified by the claim that "the include graphs do not
// meet". THEY DO: GameBridgeGameStateToX.cpp includes BrnGameModule.hpp, which reaches
// GameSource/Gui/BrnGuiEventTypeDefs.h, so the placeholders were a straight C2011 type
// redefinition against the real homes -- and that redefinition, plus a stale `mpCgsGuiModule`
// reference, is why this whole TU (and therefore the ~700-case
// TranslateGameActionsToGuiEvents) could not be compiled or mounted.
//   * GuiTakedownEvent      -> GameSource/Gui/BrnGuiEventTypeDefs.h (id 363, 40 bytes, real
//                              DWARF member names; the store map that used to live in this
//                              comment block is reproduced in that header's banner).
//   * GuiSoftTakedownEvent  -> GameSource/Gui/BrnGuiDemangledEventTypes.h (id 364, 32 bytes,
//                              still an OPAQUE `GuiEvent<364> { u8 maPayload[20]; }` shell --
//                              see the shared_header_request on the soft-takedown arm in the
//                              .cpp; the asm proves the record has NO 12-byte GuiEvent header).
// One type, one home. Do not re-fork either.

namespace CgsSystem { class TimerStatusInterface; }
namespace CgsModule { struct Event; }
namespace BrnGameState { namespace GameStateModuleIO { struct OutputBuffer; } }
namespace CgsGui { namespace CgsGuiModuleIO { struct InputBuffer; } }

namespace BrnGame
{
    // @0x823AA3B8 -- map a training-tip enum to its GUI string-ID.
    // Returns "ERROR - UNKNOWN TRAINING TYPE" for the unused/gap indices.
    const char* ConvertTrainingTypeToStringId(BrnProgression::ETrainingType leTrainingType);

    // [stuntrace wave E1, 2026-08-26] The event-flow slice of TranslateGameActionsToGuiEvents
    // @0x823E9CE0 (actions 23/37/38/39/44/47/200/201). Called from the drain walk's default arm
    // in GameBridgeGameStateToX_StuntGuiEvents.cpp; returns true when it consumed the action.
    // Body: GameBridgeGameStateToX_EventFlowGuiEvents.cpp.
    bool TranslateEventFlowGameActionToGuiEvent(
        s32 liActionType,
        const CgsModule::Event* lpAction,
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput);

    // [stuntrace wave E1, 2026-08-26] The event score/timer slice of BridgeGameStateToGui
    // @0x823EE880 (GuiEventCurrentStatus 492 / GuiEventScoreUpdate 424 / GuiAttackScoreUpdate
    // 428). Called in BrnGameModule's GUI leg inside the read/write-locked bracket, BEFORE
    // TranslateGameActionsToGuiEvents (the console's own order: the status builds run after the
    // queue Append @0x823EE9C4 and before the translate call @0x823EF22C).
    // Body: GameBridgeGameStateToX_EventStatusGuiEvents.cpp.
    void BridgeGameStateToGui_EventStatus(
        const CgsSystem::TimerStatusInterface*               lpTimerStatusInterface,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
        CgsGui::CgsGuiModuleIO::InputBuffer*                 lpGuiInput);

    // ⭐⭐ [event-starts producer wave 2026-08-27] The EVENT-START TABLE slice of
    // BridgeGameStateToGui @0x823EE880 -- its @0x823EF1A0..0x823EF1DC arm:
    //     if (lpGameStateOutput->GetSetUpAllEventStartsInterfaceIsValid())
    //         { memcpy(local, out + 0x2B0F0, 0x20E0); AddGuiEvent<GuiEventUpdateEventStarts>(...); }
    // i.e. the ONE hop that carries the table GameStateModule::SendSetUpAllEventStartsMessage
    // publishes over to GuiCache::RecEvent's case-203 arm. Called from BrnGameModule's GUI leg
    // inside the same read/write-locked bracket as the status slice above, in the console's own
    // order (this arm sits between the online-post-event build and TranslateGameActionsToGuiEvents
    // @0x823EF22C).
    // Body: GameBridgeGameStateToX_EventStartsGuiEvents.cpp.
    void BridgeGameStateToGui_EventStarts(
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
        CgsGui::CgsGuiModuleIO::InputBuffer*                 lpGuiInput);

} // namespace BrnGame

#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"   // CgsGui::CgsGuiModuleIO::InputBuffer::GetGuiEvents()
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT

namespace BrnGame
{
    // =========================================================================
    // [gateui] ONE SHARED GUI-EVENT PUSH, used by every producer in the
    // GameState->Gui bridge family (this header's two TUs).
    //
    // The console pushes each of these through `CgsGui::GuiModule::AddGuiEvent<T>` -- a
    // NON-STATIC member of the CgsGui::GuiModule embedded in BrnGameModule at +7252512 (the
    // `add r3, r29, r30` before every one of the calls in TranslateGameActionsToGuiEvents
    // @0x823E9CE0). Every one of the 270 instantiations has the SAME three-step body, e.g.
    // AddGuiEvent<GuiEventStuntInfo> @0x823D3B38:
    //     assert(lpBuffer);                                   // "Input hasn't been locked for write"
    //     queue = InputBuffer::GetGuiEvents(lpBuffer);        // sub_8284F238
    //     queue->AddEvent(&event, <T's id>, <sizeof(T)>);
    // -- i.e. the WHOLE object, at offset 0, with no header stripped. It never reads `this`.
    //
    // ⚠️ THE OBJECT IS THE PROBLEM, NOT THE BODY. Nothing on this build constructs that
    // embedded CgsGui::GuiModule (BrnGameModule.hpp says so at the +7252512 note), and the
    // header's OTHER, STATIC `AddGuiEvent(T&, InputBuffer*)` overload is NOT interchangeable:
    // it pushes `&event + 12` with size `sizeof(T) - 12`, which is correct only for payloads
    // that derive from CgsGui::GuiEvent<N>. Every type this TU posts is a PLAIN record whose
    // own GetEventType() carries the id (GuiTakedownEvent 363/40, GuiEventStuntInfo 217/12,
    // GuiEventBoostBarStuntInfo 218/12, GuiEventStuntAreaComplete 219/8,
    // GuiEventStuntAllComplete 220/4, GuiAutosaveRequestEvent 356/1 -- every (id,size) pair
    // read straight off its instantiation's asm), so that arithmetic would push a 1-byte
    // marker instead of a 12-byte record.
    //
    // So the queue is written DIRECTLY, exactly as the instantiation's own body does. This is
    // the established in-tree idiom for this situation -- BrnGameModule.cpp's GuiEventTimeInfo
    // publish carries the identical ⚠️ banner and the identical three lines. It adds no
    // dependency on CgsGuiModule_AddGuiEvent_Inst.cpp, so it needs no new mount line.
    // DELETE-WHEN the embedded CgsGui::GuiModule is constructed on PC: then these become
    // `mCgsGuiModule.AddGuiEvent(&lEvent, lpGuiInput)` verbatim.
    // =========================================================================
    template <class GuiEventT>
    static void PushGuiEvent(const GuiEventT& lrEvent,
                             CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput)
    {
        CGS_ASSERT(lpGuiInput != 0, "Input hasn't been locked for write");
        if (lpGuiInput == 0)
        {
            return;
        }
        lpGuiInput->GetGuiEvents()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lrEvent),
            lrEvent.GetEventType(),
            static_cast<s32>(sizeof(GuiEventT)));
    }
} // namespace BrnGame
