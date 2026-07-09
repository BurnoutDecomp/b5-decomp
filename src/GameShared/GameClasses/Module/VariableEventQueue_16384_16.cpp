#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_CoarseQueryQueue.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<16384, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (out-of-line per-instantiation emission).
// Shared generic bodies live in CgsVariableEventQueue.h. Per-method so siblings not
// attributed to this instance stay un-instantiated.
// Ledger: 14 methods -- this is the "safe-API" instance: it emits the Safe variants
// (AddEventSafe / AddStringEventSafe / AllocateEventSafe) alongside the bring-up and
// accessor set, but NOT Prepare/Release/GetMaxLength.

template void   CgsModule::VariableEventQueue<16384, 16>::Construct();
template void   CgsModule::VariableEventQueue<16384, 16>::Clear();
template void   CgsModule::VariableEventQueue<16384, 16>::Destruct();
template s32    CgsModule::VariableEventQueue<16384, 16>::GetLength() const;
template s32    CgsModule::VariableEventQueue<16384, 16>::GetSizeInBytes() const;
template s32    CgsModule::VariableEventQueue<16384, 16>::GetEventPaddingSize(s32) const;
template s32    CgsModule::VariableEventQueue<16384, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32    CgsModule::VariableEventQueue<16384, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool   CgsModule::VariableEventQueue<16384, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template bool   CgsModule::VariableEventQueue<16384, 16>::AddEventSafe(const CgsModule::Event*, s32, s32);
template void*  CgsModule::VariableEventQueue<16384, 16>::AllocateEventSafe(s32, s32);
template bool   CgsModule::VariableEventQueue<16384, 16>::AddStringEventSafe(const char*, s32);
template void   CgsModule::VariableEventQueue<16384, 16>::OutputQueueContents() const;
template char*  CgsModule::VariableEventQueue<16384, 16>::GetFirstWritePointer();
template const char* CgsModule::VariableEventQueue<16384, 16>::GetFirstWritePointer() const;

// ==== VEQ typed-AddEvent<EventT> family (Pass-A re-home finish) ====
// Explicit instantiation of the templated AddEvent<EventT>/Append<SRCBUF,16>/
// AppendSafe<SRCBUF,16> members the X360 emits out-of-line for THIS <16384,16>
// instance. Bodies are the shared generic templates in CgsVariableEventQueue.h
// (typed AddEvent = assert-then-forward with sizeof(EventT); Append = bulk
// memcpy; AppendSafe = per-event GetFirst/Next + AddEventSafe). Element-type
// homes are #included above so each EventT is a complete type.
template bool CgsModule::VariableEventQueue<16384, 16>::AddEvent<BrnAI::AIModuleIO::ActivateRaceCarEvent>(const BrnAI::AIModuleIO::ActivateRaceCarEvent*, s32);
template bool CgsModule::VariableEventQueue<16384, 16>::AddEvent<BrnAI::AIModuleIO::AddCarToCurrentModeEvent>(const BrnAI::AIModuleIO::AddCarToCurrentModeEvent*, s32);
template bool CgsModule::VariableEventQueue<16384, 16>::AddEvent<BrnAI::AIModuleIO::DeactivateRaceCarEvent>(const BrnAI::AIModuleIO::DeactivateRaceCarEvent*, s32);
template bool CgsModule::VariableEventQueue<16384, 16>::AddEvent<BrnAI::AIModuleIO::DetachAIControlEvent>(const BrnAI::AIModuleIO::DetachAIControlEvent*, s32);
template bool CgsModule::VariableEventQueue<16384, 16>::AddEvent<BrnAI::AIModuleIO::PlayerControlChangedEvent>(const BrnAI::AIModuleIO::PlayerControlChangedEvent*, s32);
template bool CgsModule::VariableEventQueue<16384, 16>::AddEvent<BrnAI::AIModuleIO::RemoveCarFromCurrentModeEvent>(const BrnAI::AIModuleIO::RemoveCarFromCurrentModeEvent*, s32);
template bool CgsModule::VariableEventQueue<16384, 16>::AddEvent<BrnAI::AIModuleIO::SetUpOutOfRangeRaceCarEvent>(const BrnAI::AIModuleIO::SetUpOutOfRangeRaceCarEvent*, s32);
template bool CgsModule::VariableEventQueue<16384, 16>::AddEvent<CgsSceneManager::SceneManagerIO::InEventFrustumTestVp>(const CgsSceneManager::SceneManagerIO::InEventFrustumTestVp*, s32);
template bool CgsModule::VariableEventQueue<16384, 16>::Append<16384, 16>(const CgsModule::VariableEventQueue<16384, 16>&);
template bool CgsModule::VariableEventQueue<16384, 16>::Append<512, 16>(const CgsModule::VariableEventQueue<512, 16>&);
