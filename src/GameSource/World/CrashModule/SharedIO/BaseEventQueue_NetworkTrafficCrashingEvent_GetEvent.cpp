#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::GetEvent (inline generic)
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleTrafficIOInterfaces.h" // BrnWorld::CrashIO::NetworkTrafficCrashingEvent (2-byte element)

// CgsModule::BaseEventQueue<BrnWorld::CrashIO::NetworkTrafficCrashingEvent>::GetEvent(s32) const  @ 0x82709EF8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked element accessor body is already inline
// in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// BrnTraffic::TrafficEntityModule::HandleCrashingNetworkTraffic to walk the
// CrashNetworkTrafficQueue (EventQueue<NetworkTrafficCrashingEvent,160>).
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:272), liIndex < GetLength() (:274)
// and liIndex >= 0 (:275) -- the CONST GetEvent(int) overload (:270) -- then returns
// &mpEvents[liIndex] (`result = 2*a2 + mpEvents`, `slwi r11, liIndex, 1`). The 2-byte stride is
// sizeof(NetworkTrafficCrashingEvent) == sizeof(u16 muVehicleId) == 2. The Hex-Rays `int` return
// is the ABI-returned T& pointer; the DWARF gives the real `const NetworkTrafficCrashingEvent&`.
template const BrnWorld::CrashIO::NetworkTrafficCrashingEvent&
CgsModule::BaseEventQueue<BrnWorld::CrashIO::NetworkTrafficCrashingEvent>::GetEvent(s32) const;
