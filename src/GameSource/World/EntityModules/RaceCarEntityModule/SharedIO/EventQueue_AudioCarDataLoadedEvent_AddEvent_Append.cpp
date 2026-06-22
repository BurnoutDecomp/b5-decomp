#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

// Explicit-instantiation TU for the AudioCarDataLoadedEvent-element BaseEventQueue<T>
// methods the X360 build emitted out-of-line. The generic bodies are committed inline
// in CgsBaseEventQueue.h (one body covers all element types); this TU only forces the
// per-instantiation emission. The owning queue is EventQueue<AudioCarDataLoadedEvent,16>
// (its Construct lives in the sibling EventQueue_AudioCarDataLoadedEvent_16.cpp).
//
// AudioCarDataLoadedEvent stride is 24 bytes == sizeof(AudioCarDataLoadedEvent):
//   CgsModule::Event (empty base, 0) + EMessageType meMessageType (4)
//   + const VehicleListEntry* mpVehicleListEntry (4) + CgsID mAssetID (8, 8-aligned)
//   + u8 miActiveRaceCarIndex (1) + bool mbIsPlayer (1) -> padded to 24.
// This matches the X360 `24 * a1[2]` element address arithmetic in both bodies.

// CgsModule::BaseEventQueue<BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent>::AddEvent  @ 0x822C8070
// Appends UNCONDITIONALLY (the two asserts -- "mpEvents != NULL" CgsBaseEventQueue.h:312
// and "...Reached Max length" :313 -- are non-gating tripwires): stores *a2 at
// mpEvents[miLength] as three 8-byte copies (stride 24), bumps miLength, returns true.
template bool
CgsModule::BaseEventQueue<BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent>::AddEvent(
    const BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent&);

// CgsModule::BaseEventQueue<BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent>::Append  @ 0x822CAA80
// Merges all live events from the source queue onto this queue's tail: asserts
// "mpEvents != NULL" (:413), "Base event queue overflow" (:414) and the source's
// "mpEvents != NULL" (:486, via GetQueueStartPointer), then XMemCpy's
// 24*source.miLength bytes from source.mpEvents to mpEvents + 24*miLength and
// advances miLength by source.miLength. Returns true.
template bool
CgsModule::BaseEventQueue<BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent>::Append(
    const CgsModule::BaseEventQueue<BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent>&);
