#include "GameSource/Jobs/Traffic/TrafficCommon.h"
#include <stddef.h>
typedef char check_update_size[sizeof(BrnTraffic::UpdateVehiclesJobParams)==0xa0?1:-1];
typedef char check_union_size[sizeof(BrnTraffic::JobParams)==0xa0?1:-1];
typedef char check_update_align[__alignof(BrnTraffic::UpdateVehiclesJobParams)==16?1:-1];
typedef char check_union_align[__alignof(BrnTraffic::JobParams)==16?1:-1];
typedef char check_random[offsetof(BrnTraffic::UpdateVehiclesJobParams,mEffectRand)==0x40?1:-1];
typedef char check_center[offsetof(BrnTraffic::UpdateVehiclesJobParams,mBehaviourCentre)==0x80?1:-1];
typedef char check_output[offsetof(BrnTraffic::UpdateVehiclesJobParams,mpOutNewPhysicalRequests)==0x90?1:-1];
