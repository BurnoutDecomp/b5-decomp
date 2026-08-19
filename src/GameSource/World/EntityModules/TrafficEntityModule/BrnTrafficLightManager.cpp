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

namespace
{
    // [DIAG] NOT IN THE X360 BINARY. Backing state for Q7Diag_GetLastResolvedLightIndex /
    // ...InstanceID (declared in BrnTrafficLightManager.h -- see the banner there for why this
    // is a file-static + free function rather than a member). Written only by the two
    // GotSmashed/GotRestored bodies below, read only by the [Q7-tlight] one-shot in
    // BrnTrafficEntityModule_wQ7_01.cpp. No console counterpart.
    s32 gQ7DiagLastResolvedLightIndex      = -1;
    u32 gQ7DiagLastResolvedLightInstanceID = 0;
}

// [DIAG] NOT IN THE X360 BINARY -- see BrnTrafficLightManager.h.
s32 Q7Diag_GetLastResolvedLightIndex()      { return gQ7DiagLastResolvedLightIndex; }
u32 Q7Diag_GetLastResolvedLightInstanceID() { return gQ7DiagLastResolvedLightInstanceID; }

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

        // [DIAG] NOT IN THE X360 BINARY -- record the resolution for the [Q7-tlight] one-shot.
        gQ7DiagLastResolvedLightIndex      = liInstanceIndex;
        gQ7DiagLastResolvedLightInstanceID = luInstanceID;
    }
}

// -- TrafficLightGotRestored @ 0x82751A40 (40 insns) --------------------------
//
// The paired "this traffic light is back" event: the same id-hash resolution as
// TrafficLightGotSmashed, then CLEAR the smashed bit instead of setting it. Measured
// instruction for instruction (the address has no .ida-exports JSON -- exporter-run gap,
// gotcha 6 -- so it was dumped with headless idat on a private .i64 copy,
// scratchpad/waveQ7/ida_tl/out.json, fn_0x82751A40):
//
//   cmplwi r31,0 / bne              -> assert("lpTrafficLightData")            :0x1BB (443)
//   bl GetInstanceIndexForInstanceID
//   cmpwi  r4,-1 / beq  loc_82751AD8-> a missing id is silently ignored
//   bl GetLightState
//   cmplwi r31,0 / bne              -> assert("lpState")                        :0x1C3 (451)
//   lbz    r11,5(r31)
//   clrlwi r11,r11,25               -> KEEP bits 25..31 == muFlags &= 0x7F      (the ONE
//                                      instruction that differs from the smashed twin,
//                                      which sets 0x80 instead)
//   stb    r11,5(r31)
//
// The baked d:\p4 path + the two line numbers are dropped per project policy; the assert
// STRINGS are the X360 rodata literals verbatim.
void TrafficLightManager::TrafficLightGotRestored(const TrafficLightCollection* lpTrafficLightData,
                                                  u32 luInstanceID)
{
    CGS_ASSERT(lpTrafficLightData, "lpTrafficLightData");

    const s32 liInstanceIndex = lpTrafficLightData->GetInstanceIndexForInstanceID(luInstanceID);
    if (liInstanceIndex != -1)
    {
        TrafficLightState* lpState = GetLightState(static_cast<u32>(liInstanceIndex));
        CGS_ASSERT(lpState, "lpState");

        // Clear the smashed bit (0x80) of the record's flags byte at +5, leaving the low
        // 7 bits (the active-state mask the corona render legs consume) untouched. Byte
        // access mirrors the smashed twin above and keeps this slice additive over the
        // placeholder TrafficLightState element type.
        reinterpret_cast<u8*>(lpState)[5] &= 0x7Fu;

        // [DIAG] NOT IN THE X360 BINARY -- record the resolution for the [Q7-tlight] one-shot.
        gQ7DiagLastResolvedLightIndex      = liInstanceIndex;
        gQ7DiagLastResolvedLightInstanceID = luInstanceID;
    }
}

}
