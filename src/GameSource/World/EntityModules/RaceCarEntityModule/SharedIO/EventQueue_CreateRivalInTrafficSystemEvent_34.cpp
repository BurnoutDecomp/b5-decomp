#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarToTrafficInterface.h"

// CgsModule::EventQueue<BrnWorld::RaceCarEntityModuleIO::CreateRivalInTrafficSystemEvent, 34>::Construct
//   @ 0x822E34B0. Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (34) queue of
// CreateRivalInTrafficSystemEvent (sizeof == 48: 2x Vector3 SIMD (16+16) + EDistrict(4) +
// EGlobalRaceCarIndex(4) + s8 -> 48 with 16-byte alignment). The element type is alignas(16),
// so the inline maEvents buffer lands at this+0x10 (12-byte BaseEventQueue base + 4 pad) --
// exactly the constructor's:
//   addi r30, r31, 0x10   ; mpEvents = &maEvents
//   stw  0x22 (=34), 4    ; miMaxLength = KI_LENGTH (34)
//   stw  0,       8        ; miLength   = 0
// (the `lpEventBuffer != NULL` assert @ CgsBaseEventQueue.h:160 is a non-gating tripwire).
//
// The generic EventQueue<T,N>::Construct / BaseEventQueue<T>::Construct bodies are already
// committed inline in CgsEventQueue.h / CgsBaseEventQueue.h -- this TU is the explicit
// instantiation ONLY (do NOT re-define the generic). The sibling
// EventQueue_RemoveRivalFromTrafficSystemEvent_34.cpp (@0x822E3520) is the matching template.
//   Caller (X360): BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene::Construct,
//                  BrnTraffic::BrnTrafficIO::InputBuffer_PostScene::Construct.
template void CgsModule::EventQueue<BrnWorld::RaceCarEntityModuleIO::CreateRivalInTrafficSystemEvent, 34>::Construct();
