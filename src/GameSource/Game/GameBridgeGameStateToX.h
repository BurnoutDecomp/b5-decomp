#pragma once
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX.h
//
// Public declarations for the GameState->X bridge TU (declared home
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
    // BrnGameModule::BridgeGameStateToController (the console's single accessor returns the
    // bind-request queue; the unbind-request queue sits at +0x4C from it). Their canonical home
    // is the game-state IO; declared here so the bridge TU resolves them until that TU lands.
    const CgsInput::InputIO::PostWorldInputBuffer::BindRequestQueue*
        GetGameStateInputBindRequestQueue(GameStateModule* lpGameStateOutput);
    const CgsInput::InputIO::PostWorldInputBuffer::UnBindRequestQueue*
        GetGameStateInputUnbindRequestQueue(GameStateModule* lpGameStateOutput);

    // The takedown-event output queue TranslateTakedownsToGuiEvents walks is NOT declared here:
    // it is the committed CgsModule::EventQueue<BrnGameState::TakedownEvent, 8> that
    // BridgeGameStateToWorld already names (GameSource/GameState/TakedownManager/
    // BrnTakedownManagerTypes.h + EventQueue_TakedownEvent_8.cpp). The bridge takes it as a
    // const void* because that is how BrnGameModule.hpp declares the method, and re-types it
    // with one documented cross-home cast.
}

// The two GUI-event placeholders that used to live in this header are gone. One type, one
// home: BrnGui::GuiTakedownEvent (id 363, 40 bytes) and BrnGui::GuiSoftTakedownEvent (id 364,
// 32 bytes) are both defined in GameSource/Gui/BrnGuiEventTypeDefs.h with their real member
// names and layout static_asserts. Do not re-fork either.

namespace CgsSystem { class TimerStatusInterface; }
namespace CgsModule { struct Event; }
namespace BrnGameState { namespace GameStateModuleIO { struct OutputBuffer; } }
namespace CgsGui { namespace CgsGuiModuleIO { struct InputBuffer; } }

namespace BrnGame
{
    // Map a training-tip enum to its GUI string-ID.
    // Returns "ERROR - UNKNOWN TRAINING TYPE" for the unused/gap indices.
    const char* ConvertTrainingTypeToStringId(BrnProgression::ETrainingType leTrainingType);

    // [stuntrace wave E1, 2026-08-26] The event-flow slice of TranslateGameActionsToGuiEvents
    // (actions 23/37/38/39/44/47/200/201). Called from the drain walk's default arm
    // in GameBridgeGameStateToX_StuntGuiEvents.cpp; returns true when it consumed the action.
    // Body: GameBridgeGameStateToX_EventFlowGuiEvents.cpp.
    bool TranslateEventFlowGameActionToGuiEvent(
        s32 liActionType,
        const CgsModule::Event* lpAction,
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput);

    // [stuntrace wave E1, 2026-08-26] The event score/timer slice of BridgeGameStateToGui
    // (GuiEventCurrentStatus 492 / GuiEventScoreUpdate 424 / GuiAttackScoreUpdate
    // 428). Called in BrnGameModule's GUI leg inside the read/write-locked bracket, BEFORE
    // TranslateGameActionsToGuiEvents (the console's own order: the status builds run after the
    // queue Append and before the translate call).
    // Body: GameBridgeGameStateToX_EventStatusGuiEvents.cpp.
    void BridgeGameStateToGui_EventStatus(
        const CgsSystem::TimerStatusInterface*               lpTimerStatusInterface,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
        CgsGui::CgsGuiModuleIO::InputBuffer*                 lpGuiInput);

    // ⭐⭐ [event-starts producer wave 2026-08-27] The EVENT-START TABLE slice of
    // BridgeGameStateToGui -- its event-start-table arm:
    //     if (lpGameStateOutput->GetSetUpAllEventStartsInterfaceIsValid())
    //         { memcpy(local, out + 0x2B0F0, 0x20E0); AddGuiEvent<GuiEventUpdateEventStarts>(...); }
    // i.e. the ONE hop that carries the table GameStateModule::SendSetUpAllEventStartsMessage
    // publishes over to GuiCache::RecEvent's case-203 arm. Called from BrnGameModule's GUI leg
    // inside the same read/write-locked bracket as the status slice above, in the console's own
    // order (this arm sits between the online-post-event build and TranslateGameActionsToGuiEvents).
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
    // `add r3, r29, r30` before every one of the calls in TranslateGameActionsToGuiEvents).
// Every one of the 270 instantiations has the SAME three-step body, e.g.
    // AddGuiEvent<GuiEventStuntInfo>:
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
