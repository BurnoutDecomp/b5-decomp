#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficAIInterfaces.h"

// CgsModule::EventQueue<BrnTraffic::BrnTrafficIO::RivalInTrafficUpdateEvent, 34>::Construct
//   @ 0x82760750. Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (34) queue of
// RivalInTrafficUpdateEvent. Construct points the base queue at its inline maEvents buffer
// (at struct offset 16 -- the 12-byte BaseEventQueue base padded to 16 by the element's
// 16-byte Vector3 SIMD alignment, store-for-store: stw maEvents@0, stw 34@4, stw 0@8), sets
// KI_LENGTH(34) and clears the live count. The generic EventQueue<T,N>::Construct /
// BaseEventQueue<T>::Construct bodies are already committed inline in CgsEventQueue.h /
// CgsBaseEventQueue.h -- this TU is the explicit instantiation ONLY (do NOT re-define the generic).
template void CgsModule::EventQueue<BrnTraffic::BrnTrafficIO::RivalInTrafficUpdateEvent, 34>::Construct();
