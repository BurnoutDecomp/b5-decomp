#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<32768, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (out-of-line per-instantiation emission).
// Shared generic bodies live in CgsVariableEventQueue.h. Per-method so safe siblings
// not attributed to this instance stay un-instantiated.
// Ledger: 14 methods (the full Construct/Prepare/Release set PLUS AllocateEvent).

template void   CgsModule::VariableEventQueue<32768, 16>::Construct();
template void   CgsModule::VariableEventQueue<32768, 16>::Clear();
template bool   CgsModule::VariableEventQueue<32768, 16>::Prepare();
template bool   CgsModule::VariableEventQueue<32768, 16>::Release();
template void   CgsModule::VariableEventQueue<32768, 16>::Destruct();
template s32    CgsModule::VariableEventQueue<32768, 16>::GetLength() const;
template s32    CgsModule::VariableEventQueue<32768, 16>::GetSizeInBytes() const;
template s32    CgsModule::VariableEventQueue<32768, 16>::GetEventPaddingSize(s32) const;
template s32    CgsModule::VariableEventQueue<32768, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32    CgsModule::VariableEventQueue<32768, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool   CgsModule::VariableEventQueue<32768, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template void*  CgsModule::VariableEventQueue<32768, 16>::AllocateEvent(s32, s32);
template void   CgsModule::VariableEventQueue<32768, 16>::OutputQueueContents() const;
template char*  CgsModule::VariableEventQueue<32768, 16>::GetFirstWritePointer();
template const char* CgsModule::VariableEventQueue<32768, 16>::GetFirstWritePointer() const;

// ==== VEQ typed-AddEvent<EventT> family (Pass-A re-home finish) ====
// Explicit instantiation of the templated AddEvent<EventT>/Append<SRCBUF,16>/
// AppendSafe<SRCBUF,16> members the X360 emits out-of-line for THIS <32768,16>
// instance. Bodies are the shared generic templates in CgsVariableEventQueue.h
// (typed AddEvent = assert-then-forward with sizeof(EventT); Append = bulk
// memcpy; AppendSafe = per-event GetFirst/Next + AddEventSafe). Element-type
// homes are #included above so each EventT is a complete type.
template bool CgsModule::VariableEventQueue<32768, 16>::AddEvent<BrnResource::GameDataIO::GetFreeburnChallengeListRequest>(const BrnResource::GameDataIO::GetFreeburnChallengeListRequest*, s32);
template bool CgsModule::VariableEventQueue<32768, 16>::AddEvent<BrnResource::GameDataIO::GetVehicleListRequest>(const BrnResource::GameDataIO::GetVehicleListRequest*, s32);
template bool CgsModule::VariableEventQueue<32768, 16>::AddEvent<CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult>(const CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult*, s32);
// AddEvent<OutEventLineTestFastDoubleSidedResult> @ X360 0x828D0780 -- assert-then-forward
// with sizeof(EventT) == 8 (asm r6=8). Produced by SceneManagerModule::ProcessLineTestFastDoubleSided
// / ProcessSphereTestFast. Element home: CgsSceneManagerModuleIO.h (included above).
template bool CgsModule::VariableEventQueue<32768, 16>::AddEvent<CgsSceneManager::SceneManagerIO::OutEventLineTestFastDoubleSidedResult>(const CgsSceneManager::SceneManagerIO::OutEventLineTestFastDoubleSidedResult*, s32);
template bool CgsModule::VariableEventQueue<32768, 16>::Append<1024, 16>(const CgsModule::VariableEventQueue<1024, 16>&);
template bool CgsModule::VariableEventQueue<32768, 16>::Append<18432, 16>(const CgsModule::VariableEventQueue<18432, 16>&);
template bool CgsModule::VariableEventQueue<32768, 16>::Append<2048, 16>(const CgsModule::VariableEventQueue<2048, 16>&);
template bool CgsModule::VariableEventQueue<32768, 16>::Append<256, 16>(const CgsModule::VariableEventQueue<256, 16>&);
template bool CgsModule::VariableEventQueue<32768, 16>::Append<3072, 16>(const CgsModule::VariableEventQueue<3072, 16>&);
template bool CgsModule::VariableEventQueue<32768, 16>::Append<32768, 16>(const CgsModule::VariableEventQueue<32768, 16>&);
template bool CgsModule::VariableEventQueue<32768, 16>::Append<4096, 16>(const CgsModule::VariableEventQueue<4096, 16>&);
template bool CgsModule::VariableEventQueue<32768, 16>::Append<512, 16>(const CgsModule::VariableEventQueue<512, 16>&);
