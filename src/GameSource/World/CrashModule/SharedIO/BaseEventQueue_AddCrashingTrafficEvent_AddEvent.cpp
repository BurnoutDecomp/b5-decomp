#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleTrafficIOInterfaces.h" // BrnWorld::CrashIO::AddCrashingTrafficEvent (16-byte element)

// CgsModule::BaseEventQueue<BrnWorld::CrashIO::AddCrashingTrafficEvent>::AddEvent @ 0x8271A2E8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. The X360 body
// asserts mpEvents != NULL (CgsBaseEventQueue.h:312) and miLength < miMaxLength
// (CgsBaseEventQueue.h:313) -- both non-gating tripwires whose StrStream message formatting
// collapses to the house CGS_ASSERT -- then appends the element unconditionally and bumps
// miLength. Element stride is 16 bytes: the store is `slwi r,miLength,4; add mpEvents` followed
// by two 8-byte stores (ld/std @0 + ld/std @8), i.e. the whole 16-byte element copy.
// sizeof(AddCrashingTrafficEvent) == VolumeInstanceId(u64, 8-byte aligned)(8) + EntityId(4)
// + eCrashTrafficType(4) == 16, matching the stride. Called by
// BrnTraffic::TrafficEntityModule::GenerateVehicleCrashedEvents (@0x82727768).
template bool
CgsModule::BaseEventQueue<BrnWorld::CrashIO::AddCrashingTrafficEvent>::AddEvent(
    const BrnWorld::CrashIO::AddCrashingTrafficEvent&);
