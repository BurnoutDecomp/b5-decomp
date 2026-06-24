#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::AddEvent/Append (inline generic)
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleTrafficIOInterfaces.h" // BrnWorld::CrashIO::NetworkTrafficCrashingEvent (2-byte element)

// CgsModule::BaseEventQueue<BrnWorld::CrashIO::NetworkTrafficCrashingEvent>::AddEvent @ 0x827C2D48
// CgsModule::BaseEventQueue<BrnWorld::CrashIO::NetworkTrafficCrashingEvent>::Append  @ 0x827A8570
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent/Append
// bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit instantiations.
//
// AddEvent (0x827C2D48) asserts mpEvents != NULL (CgsBaseEventQueue.h:312) and
// miLength < miMaxLength (CgsBaseEventQueue.h:313, the StrStream "Reached Max length" tripwire
// that collapses to the house CGS_ASSERT) -- both non-gating -- then appends the element
// unconditionally and bumps miLength. Element stride is 2 bytes: the X360 body does
// `lhz r10,0(src); slwi r11,miLength,1; sthx r10,r11,mpEvents; addi miLength,1` -- a single
// 16-bit store at a 2-byte stride (matching the generic `mpEvents[miLength] = lEvent`).
//
// Append (0x827A8570) asserts mpEvents != NULL (CgsBaseEventQueue.h:413), the combined length
// will not overflow (:414, "Base event queue overflow"), and the source owns a buffer (:486 via
// GetQueueStartPointer) -- all non-gating -- then block-copies the source's live events
// (XMemCpy with `slwi count,miLength,1` == 2*count bytes, dest `slwi,1` == mpEvents + 2*miLength)
// and advances miLength by source's miLength. Returns 1.
//
// sizeof(NetworkTrafficCrashingEvent) == 2 (single u16 muVehicleId), matching the 2-byte stride
// in both bodies. AddEvent is called by BrnWorld::CrashModule::HandleNetworkCrashingTraffic;
// Append by BrnTraffic::BrnTrafficIO::InputBuffer_PostScene::SetCrashTrafficOutputInterface.
template bool
CgsModule::BaseEventQueue<BrnWorld::CrashIO::NetworkTrafficCrashingEvent>::AddEvent(
    const BrnWorld::CrashIO::NetworkTrafficCrashingEvent&);

template bool
CgsModule::BaseEventQueue<BrnWorld::CrashIO::NetworkTrafficCrashingEvent>::Append(
    const CgsModule::BaseEventQueue<BrnWorld::CrashIO::NetworkTrafficCrashingEvent>&);
