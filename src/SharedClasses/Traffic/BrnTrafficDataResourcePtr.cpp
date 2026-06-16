#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"   // BrnTraffic::TrafficData

// Per-instantiation TU for CgsResource::ResourcePtr<BrnTraffic::TrafficData>.
// Bodies are the generic inline accessors in CgsResourcePtr.h; this .cpp only forces
// the out-of-line emission of the three symbols the X360 ARTIST build attested.
//
//   operator->()        @ 0x82719B90  (baked assert line 544)
//   operator*()         @ 0x82211C60  (baked assert line 563)
//   GetMemoryResource() @ 0x82211D00  (baked assert line 581)
template BrnTraffic::TrafficData* CgsResource::ResourcePtr<BrnTraffic::TrafficData>::operator->();
template BrnTraffic::TrafficData& CgsResource::ResourcePtr<BrnTraffic::TrafficData>::operator*();
template BrnTraffic::TrafficData* CgsResource::ResourcePtr<BrnTraffic::TrafficData>::GetMemoryResource();
