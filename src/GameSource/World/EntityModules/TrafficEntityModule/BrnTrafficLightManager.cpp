#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"
#include "SharedClasses/Traffic/Junctions/BrnTrafficLightCollection.h"   // TrafficLightCollection::GetInstanceIndexForInstanceID

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

// -- TrafficLightGotSmashed @ 0x827519A0 --------------------------------------
//
// A prop-smash event (from TrafficEntityModule::HandlePropModuleRequests) knocks over a
// traffic light: resolve the persistent instance id to its dense index via the baked
// collection's id hash, then flag the matching runtime state record as smashed by setting
// the top bit (0x80) of its flags byte (record+5 == TrafficLightRuntimeState::muFlags).
// A missing id (index == -1) is silently ignored. Assert strings are the X360 rodata
// literals verbatim (baked d:\p4 path + lines 417/425 dropped per project policy).
void TrafficLightManager::TrafficLightGotSmashed(const TrafficLightCollection* lpTrafficLightData,
                                                 u32 luInstanceID)
{
    CGS_ASSERT(lpTrafficLightData, "lpTrafficLightData");

    const s32 liInstanceIndex = lpTrafficLightData->GetInstanceIndexForInstanceID(luInstanceID);
    if (liInstanceIndex != -1)
    {
        TrafficLightState* lpState = GetLightState(static_cast<u32>(liInstanceIndex));
        CGS_ASSERT(lpState, "lpState");

        // Set the smashed bit (0x80) of the record's flags byte at +5 (the attested
        // TrafficLightRuntimeState::muFlags offset). Byte access keeps this slice additive
        // over the placeholder TrafficLightState element type (see BrnTrafficLightManager.h).
        reinterpret_cast<u8*>(lpState)[5] |= 0x80u;
    }
}

}
