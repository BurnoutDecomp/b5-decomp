#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"   // TakedownEventOutputQueueType + the forwarder decl

// CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>::Construct  @ 0x822E29C0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (8) event queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>::Construct();

// CgsModule::BaseEventQueue<BrnGameState::TakedownEvent> methods (bodies inline in CgsBaseEventQueue.h).
template bool CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>::AddEvent(const BrnGameState::TakedownEvent&);
template bool CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>::Append(const CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>&);
template BrnGameState::TakedownEvent& CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>::GetEvent(s32);
// X360 0x822AC660 (class:BrnGameState catch-all TU, ledger "BrnGameState::TakedownEvent>:") = the
// const indexed accessor of this same instantiation. The 14 callers (ProcessTakedownEvents,
// ScoringSystem::UpdateTakedowns, ...) read events out of the takedown queue by index: the X360 body
// is the "mpEvents != NULL" (CgsBaseEventQueue.h:272) + "liIndex < GetLength()" (274) + "liIndex >= 0"
// (275) sequence returning &mpEvents[i] (stride 40 == sizeof(TakedownEvent)) -- the const GetEvent body.
template const BrnGameState::TakedownEvent& CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>::GetEvent(s32) const;

// [takedown wave 2026-09-03] OutputBuffer::Construct @0x82382940's `TakedownEvent<..,8>::Construct(this +
// 16448)`, reached by name from BrnGameStateModuleIO.cpp (where the type is incomplete). The opaque
// span in OutputBuffer is the console's 876 bytes; the host object must fit inside it.
namespace BrnGameState
{
namespace GameStateModuleIO
{
    static_assert(sizeof(CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>) <= (0x43AC - 0x4040),
                  "EventQueue<TakedownEvent,8> must fit the OutputBuffer's +0x4040 span");
    void ConstructTakedownEventOutputQueue(TakedownEventOutputQueueType* lpQueue)
    {
        reinterpret_cast<CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>*>(lpQueue)->Construct();
    }
}
}
