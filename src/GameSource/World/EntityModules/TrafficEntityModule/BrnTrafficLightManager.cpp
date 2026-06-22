#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"

// BrnTraffic::TrafficLightManager::GetLightState @ 0x8274F9A0
//
// Returns the address of a traffic light's 8-byte runtime state record, addressed by a flat
// instance index. The X360 build:
//   mr   this -> r30, instance -> r31
//   if (instance >= 0x258)                       cmplwi instance, 0x258 ; blt skip
//       BeginAssert/FireAssert("luInstance < KU_MAX_TRAFFIC_LIGHT_INSTANCES", file, 209)/EndAssert
//   slwi r11, instance, 3                         ; instance * 8
//   add  r3, r11, this                            ; this + 8*instance
//   return r3
//
// Reproduced with CGS_ASSERT (the baked d:\p4 path + line 209 are dropped per project policy).
// The pointer arithmetic is expressed through the named array member so there are no raw
// offset casts: &maLightStates[instance] == (u8*)this + 8*instance (8 == sizeof record).

namespace BrnTraffic
{

TrafficLightState* TrafficLightManager::GetLightState(u32 luInstance)
{
    CGS_ASSERT(luInstance < KU_MAX_TRAFFIC_LIGHT_INSTANCES,
               "luInstance < KU_MAX_TRAFFIC_LIGHT_INSTANCES");

    return &maLightStates[luInstance];
}

}
