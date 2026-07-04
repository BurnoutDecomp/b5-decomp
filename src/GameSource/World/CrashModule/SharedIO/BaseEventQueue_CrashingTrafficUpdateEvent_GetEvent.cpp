#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::GetEvent (inline generic)
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleNetworkIOInterfaces.h" // BrnWorld::CrashIO::CrashingTrafficUpdateEvent (80-byte element)

// CgsModule::BaseEventQueue<BrnWorld::CrashIO::CrashingTrafficUpdateEvent>::GetEvent(s32) const  @ 0x8254D968
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked element accessor body is already inline
// in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// BrnNetwork::TrafficManager::GetCrashingTrafficData and BrnWorld::CrashModule::
// HandleNetworkCrashingTraffic to walk a per-player crashing-traffic update queue
// (CgsModule::EventQueue<CrashingTrafficUpdateEvent, 24>).
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:272), liIndex < GetLength() (:274)
// and liIndex >= 0 (:275) -- the CONST GetEvent(int) overload (:270) -- then returns
// &mpEvents[liIndex] (`result = 80*a2 + mpEvents`, computed as liIndex*5<<4 via
// `slwi r11,liIndex,2; add r11,liIndex,r11; slwi r11,r11,4`). The 80-byte stride is
// sizeof(CrashingTrafficUpdateEvent) == alignas(16){ u16 muVehicleId + Matrix44Affine mTransform }
// == 16 (padded id) + 64 (matrix) == 80. The Hex-Rays `int` return is the ABI-returned T& pointer;
// the DWARF gives the real `const CrashingTrafficUpdateEvent&`.
template const BrnWorld::CrashIO::CrashingTrafficUpdateEvent&
CgsModule::BaseEventQueue<BrnWorld::CrashIO::CrashingTrafficUpdateEvent>::GetEvent(s32) const;
