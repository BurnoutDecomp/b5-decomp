#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysModuleIO.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTest.h"

// Explicit per-method instantiation of CgsModule::VariableEventQueue<2048, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (out-of-line per-instantiation emission).
// Shared generic bodies live in CgsVariableEventQueue.h. Per-method so safe siblings
// not attributed to this instance stay un-instantiated.
// Ledger: 11 methods (bring-up + accessors + AddEvent; NO Prepare/Release).

template void   CgsModule::VariableEventQueue<2048, 16>::Construct();
template void   CgsModule::VariableEventQueue<2048, 16>::Clear();
template void   CgsModule::VariableEventQueue<2048, 16>::Destruct();
template s32    CgsModule::VariableEventQueue<2048, 16>::GetLength() const;
template s32    CgsModule::VariableEventQueue<2048, 16>::GetSizeInBytes() const;
template s32    CgsModule::VariableEventQueue<2048, 16>::GetEventPaddingSize(s32) const;
template s32    CgsModule::VariableEventQueue<2048, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32    CgsModule::VariableEventQueue<2048, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool   CgsModule::VariableEventQueue<2048, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template void   CgsModule::VariableEventQueue<2048, 16>::OutputQueueContents() const;
template char*  CgsModule::VariableEventQueue<2048, 16>::GetFirstWritePointer();
template const char* CgsModule::VariableEventQueue<2048, 16>::GetFirstWritePointer() const;

// ==== VEQ typed-AddEvent<EventT> family (Pass-A re-home finish) ====
// Explicit instantiation of the templated AddEvent<EventT>/Append<SRCBUF,16>/
// AppendSafe<SRCBUF,16> members the X360 emits out-of-line for THIS <2048,16>
// instance. Bodies are the shared generic templates in CgsVariableEventQueue.h
// (typed AddEvent = assert-then-forward with sizeof(EventT); Append = bulk
// memcpy; AppendSafe = per-event GetFirst/Next + AddEventSafe). Element-type
// homes are #included above so each EventT is a complete type.
template bool CgsModule::VariableEventQueue<2048, 16>::AddEvent<BrnResource::GameDataIO::LoadGameDataEvent>(const BrnResource::GameDataIO::LoadGameDataEvent*, s32);
template bool CgsModule::VariableEventQueue<2048, 16>::AddEvent<BrnResource::GameDataIO::SwapInCollisionWorldRequest>(const BrnResource::GameDataIO::SwapInCollisionWorldRequest*, s32);
template bool CgsModule::VariableEventQueue<2048, 16>::AddEvent<BrnResource::GameDataIO::SwapOutCollisionWorldRequest>(const BrnResource::GameDataIO::SwapOutCollisionWorldRequest*, s32);
template bool CgsModule::VariableEventQueue<2048, 16>::AddEvent<BrnResource::GameDataIO::UnloadGameDataEvent>(const BrnResource::GameDataIO::UnloadGameDataEvent*, s32);
template bool CgsModule::VariableEventQueue<2048, 16>::AddEvent<CgsAttribSys::AttribSysIO::RegisterSchemaRequest>(const CgsAttribSys::AttribSysIO::RegisterSchemaRequest*, s32);
template bool CgsModule::VariableEventQueue<2048, 16>::AddEvent<CgsAttribSys::AttribSysIO::RegisterVaultRequest>(const CgsAttribSys::AttribSysIO::RegisterVaultRequest*, s32);
template bool CgsModule::VariableEventQueue<2048, 16>::AddEvent<CgsAttribSys::AttribSysIO::UnregisterVaultRequest>(const CgsAttribSys::AttribSysIO::UnregisterVaultRequest*, s32);
template bool CgsModule::VariableEventQueue<2048, 16>::AddEvent<CgsSceneManager::SceneManagerIO::InEventLineTestFine>(const CgsSceneManager::SceneManagerIO::InEventLineTestFine*, s32);
template bool CgsModule::VariableEventQueue<2048, 16>::Append<2048, 16>(const CgsModule::VariableEventQueue<2048, 16>&);
template bool CgsModule::VariableEventQueue<2048, 16>::Append<32768, 16>(const CgsModule::VariableEventQueue<32768, 16>&);
