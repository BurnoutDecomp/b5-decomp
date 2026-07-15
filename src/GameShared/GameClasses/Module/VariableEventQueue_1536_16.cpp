#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"      // EventQueue<EventT,N> source for the typed Append
#include "GameSource/GameState/BrnGameEvents.h"               // FinishedSyncingPlayersEvent / HitOverheadSignEvent / RecordPropHitEvent homes

// Explicit per-method instantiation of CgsModule::VariableEventQueue<1536, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the X360 emits each instantiation's
// methods out-of-line). The shared generic bodies live in CgsVariableEventQueue.h;
// these per-method lines force this instantiation's out-of-line emission. Per-method
// (not whole-class) so the declared-only safe siblings (AddEventSafe/AllocateEventSafe/
// AddStringEvent(Safe)/GetMaxLength) stay un-instantiated, matching the X360 ledger.

template void CgsModule::VariableEventQueue<1536, 16>::Construct();
template void CgsModule::VariableEventQueue<1536, 16>::Clear();
template bool CgsModule::VariableEventQueue<1536, 16>::Prepare();
template bool CgsModule::VariableEventQueue<1536, 16>::Release();
template void CgsModule::VariableEventQueue<1536, 16>::Destruct();
template s32  CgsModule::VariableEventQueue<1536, 16>::GetLength() const;
template s32  CgsModule::VariableEventQueue<1536, 16>::GetSizeInBytes() const;
template s32  CgsModule::VariableEventQueue<1536, 16>::GetEventPaddingSize(s32) const;
template s32  CgsModule::VariableEventQueue<1536, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32  CgsModule::VariableEventQueue<1536, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool CgsModule::VariableEventQueue<1536, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template void* CgsModule::VariableEventQueue<1536, 16>::AllocateEvent(s32, s32);
template void CgsModule::VariableEventQueue<1536, 16>::OutputQueueContents() const;
template char* CgsModule::VariableEventQueue<1536, 16>::GetFirstWritePointer();
template const char* CgsModule::VariableEventQueue<1536, 16>::GetFirstWritePointer() const;

// ==== VEQ typed-AddEvent<EventT> family (Pass-A re-home finish) ====
// Explicit instantiation of the templated AddEvent<EventT>/Append<SRCBUF,16>/
// AppendSafe<SRCBUF,16> members the X360 emits out-of-line for THIS <1536,16>
// instance. Bodies are the shared generic templates in CgsVariableEventQueue.h
// (typed AddEvent = assert-then-forward with sizeof(EventT); Append = bulk
// memcpy; AppendSafe = per-event GetFirst/Next + AddEventSafe). Element-type
// homes are #included above so each EventT is a complete type.
template bool CgsModule::VariableEventQueue<1536, 16>::Append<1536, 16>(const CgsModule::VariableEventQueue<1536, 16>&);

// Typed AddEvent<EventT> @ 0x82566168 -- assert-then-forward with sizeof(EventT)==1 (li r6,1 @ 0x82566204).
// Element home BrnGameEvents.h (1-byte marker, FLAG minimal-home). Called by BrnNetwork::StateManager::UpdateSyncTime.
template bool CgsModule::VariableEventQueue<1536, 16>::AddEvent<BrnGameState::GameStateModuleIO::FinishedSyncingPlayersEvent>(const BrnGameState::GameStateModuleIO::FinishedSyncingPlayersEvent*, s32);

// Typed per-event Append<EventT,SRCN> from a fixed-stride EventQueue source (walk 0..GetLength(), AddEvent each with
// sizeof(EventT)). Both @ WorldModule::BridgeEntityModulesToOutput_PostPhysics.
//   HitOverheadSignEvent,100 @ 0x827AECF8, sizeof 1  (li r6,1  @ 0x827AEDB4)
//   RecordPropHitEvent,50    @ 0x827AEC10, sizeof 32 (li r6,0x20 @ 0x827AECCC)
template bool CgsModule::VariableEventQueue<1536, 16>::Append<BrnGameState::GameStateModuleIO::HitOverheadSignEvent, 100>(const CgsModule::EventQueue<BrnGameState::GameStateModuleIO::HitOverheadSignEvent, 100>&, s32);
template bool CgsModule::VariableEventQueue<1536, 16>::Append<BrnGameState::GameStateModuleIO::RecordPropHitEvent, 50>(const CgsModule::EventQueue<BrnGameState::GameStateModuleIO::RecordPropHitEvent, 50>&, s32);
