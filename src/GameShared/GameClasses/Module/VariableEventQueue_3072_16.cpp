#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<3072, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (out-of-line per-instantiation emission).
// Shared generic bodies live in CgsVariableEventQueue.h. Per-method so the safe
// siblings the ledger does NOT attribute to this instance stay un-instantiated.
// Ledger: 10 methods (used by BrnGameState::GameStateModuleIO::OutputBuffer).
//
// BUFSIZE=3072 (0xC00): miBufferWritePos@0xC04, miLength@0xC08, miFirstEventOffset@0xC0C
// (== BUFSIZE+4/+8/+12); mbIsConstructed @+0, macData @+1; CBufferEntry stride 0x10.

template void   CgsModule::VariableEventQueue<3072, 16>::Construct();
template void   CgsModule::VariableEventQueue<3072, 16>::Clear();
template s32    CgsModule::VariableEventQueue<3072, 16>::GetLength() const;
template s32    CgsModule::VariableEventQueue<3072, 16>::GetSizeInBytes() const;
template s32    CgsModule::VariableEventQueue<3072, 16>::GetEventPaddingSize(s32) const;
template s32    CgsModule::VariableEventQueue<3072, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32    CgsModule::VariableEventQueue<3072, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool   CgsModule::VariableEventQueue<3072, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template void   CgsModule::VariableEventQueue<3072, 16>::OutputQueueContents() const;
template char*  CgsModule::VariableEventQueue<3072, 16>::GetFirstWritePointer();

// ==== VEQ typed-AddEvent<EventT> family (Pass-A re-home finish) ====
// Explicit instantiation of the templated AddEvent<EventT>/Append<SRCBUF,16>/
// AppendSafe<SRCBUF,16> members the X360 emits out-of-line for THIS <3072,16>
// instance. Bodies are the shared generic templates in CgsVariableEventQueue.h
// (typed AddEvent = assert-then-forward with sizeof(EventT); Append = bulk
// memcpy; AppendSafe = per-event GetFirst/Next + AddEventSafe). Element-type
// homes are #included above so each EventT is a complete type.
template bool CgsModule::VariableEventQueue<3072, 16>::AddEvent<BrnResource::GameDataIO::GetFreeburnChallengeListRequest>(const BrnResource::GameDataIO::GetFreeburnChallengeListRequest*, s32);
template bool CgsModule::VariableEventQueue<3072, 16>::AddEvent<BrnResource::GameDataIO::GetGameDataEvent>(const BrnResource::GameDataIO::GetGameDataEvent*, s32);
template bool CgsModule::VariableEventQueue<3072, 16>::AddEvent<BrnResource::GameDataIO::GetVehicleListRequest>(const BrnResource::GameDataIO::GetVehicleListRequest*, s32);
template bool CgsModule::VariableEventQueue<3072, 16>::AddEvent<BrnResource::GameDataIO::GetWheelListRequest>(const BrnResource::GameDataIO::GetWheelListRequest*, s32);
template bool CgsModule::VariableEventQueue<3072, 16>::AddEvent<BrnResource::GameDataIO::LoadGameDataEvent>(const BrnResource::GameDataIO::LoadGameDataEvent*, s32);
