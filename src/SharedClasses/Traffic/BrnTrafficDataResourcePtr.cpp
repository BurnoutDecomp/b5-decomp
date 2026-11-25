#define CGS_RESOURCE_PTR_MATCH_RELATIVE_ASSERTS
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#undef CGS_RESOURCE_PTR_MATCH_RELATIVE_ASSERTS
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"   // BrnTraffic::TrafficData

// Emit the TrafficData accessors from the generic ResourcePtr implementation.
// The mutable operator-> at ARTIST 0x82719B90 records the relative path and
// CgsResourcePtr.h:544.
template BrnTraffic::TrafficData* CgsResource::ResourcePtr<BrnTraffic::TrafficData>::operator->();
template BrnTraffic::TrafficData& CgsResource::ResourcePtr<BrnTraffic::TrafficData>::operator*();
template BrnTraffic::TrafficData* CgsResource::ResourcePtr<BrnTraffic::TrafficData>::GetMemoryResource();
