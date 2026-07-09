#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_CoarseQueryQueue.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRenderCommon.h"  // CgsDev::Internal::CInEventDraw* record homes

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

// ==== VEQ typed-AddEventSafe<CInEventDrawX> family (CgsDev debug-draw publishers) ====
// Explicit instantiation of the templated AddEventSafe<EventT> member the X360 emits out-of-line
// for THIS <16384,16> instance, once per CgsDev debug-draw record type. Body is the shared generic
// template in CgsVariableEventQueue.h (assert-constructed, then forward to the 3-arg AddEventSafe
// with sizeof(EventT)). Each record's sizeof is cross-checked against the size immediate (li r6)
// baked into that instance's asm via static_assert in CgsDebugRenderCommon.h (address per line).
template bool CgsModule::VariableEventQueue<16384, 16>::AddEventSafe<CgsDev::Internal::CInEventDrawText2D>(const CgsDev::Internal::CInEventDrawText2D*, s32);      // 0x82828418, size 0x10
template bool CgsModule::VariableEventQueue<16384, 16>::AddEventSafe<CgsDev::Internal::CInEventDrawBox2D>(const CgsDev::Internal::CInEventDrawBox2D*, s32);        // 0x82828588, size 0x14
template bool CgsModule::VariableEventQueue<16384, 16>::AddEventSafe<CgsDev::Internal::CInEventDrawText>(const CgsDev::Internal::CInEventDrawText*, s32);          // 0x82828640, size 0x14
template bool CgsModule::VariableEventQueue<16384, 16>::AddEventSafe<CgsDev::Internal::CInEventDrawLine>(const CgsDev::Internal::CInEventDrawLine*, s32);          // 0x828286F8, size 0x1C
template bool CgsModule::VariableEventQueue<16384, 16>::AddEventSafe<CgsDev::Internal::CInEventDrawQuad>(const CgsDev::Internal::CInEventDrawQuad*, s32);          // 0x828287B0, size 0x34
template bool CgsModule::VariableEventQueue<16384, 16>::AddEventSafe<CgsDev::Internal::CInEventDrawAxis>(const CgsDev::Internal::CInEventDrawAxis*, s32);          // 0x82828868, size 0x30
template bool CgsModule::VariableEventQueue<16384, 16>::AddEventSafe<CgsDev::Internal::CInEventDrawSphere>(const CgsDev::Internal::CInEventDrawSphere*, s32);      // 0x82828920, size 0x14
template bool CgsModule::VariableEventQueue<16384, 16>::AddEventSafe<CgsDev::Internal::CInEventDrawSolidSphere>(const CgsDev::Internal::CInEventDrawSolidSphere*, s32); // 0x828289D8, size 0x14
template bool CgsModule::VariableEventQueue<16384, 16>::AddEventSafe<CgsDev::Internal::CInEventDrawCircle>(const CgsDev::Internal::CInEventDrawCircle*, s32);      // 0x82828A90, size 0x20
template bool CgsModule::VariableEventQueue<16384, 16>::AddEventSafe<CgsDev::Internal::CInEventDrawBox>(const CgsDev::Internal::CInEventDrawBox*, s32);            // 0x82828B48, size 0x4C
template bool CgsModule::VariableEventQueue<16384, 16>::AddEventSafe<CgsDev::Internal::CInEventDrawSolidBox>(const CgsDev::Internal::CInEventDrawSolidBox*, s32);  // 0x82828C00, size 0x4C
template bool CgsModule::VariableEventQueue<16384, 16>::AddEventSafe<CgsDev::Internal::CInEventDrawArrow>(const CgsDev::Internal::CInEventDrawArrow*, s32);        // 0x82828CB8, size 0x1C
