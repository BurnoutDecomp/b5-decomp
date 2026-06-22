#ifndef BRN_TRAFFIC_LIGHT_MANAGER_H
#define BRN_TRAFFIC_LIGHT_MANAGER_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

// ---------------------------------------------------------------------------
// BrnTraffic::TrafficLightManager
//
// DWARF/asm home: GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h
// (the X360 retail XEX assert string in GetLightState cites this exact header path, line 209).
//
// Owns the per-traffic-light runtime state for the world. Lights are addressed by a flat
// instance index in [0, KU_MAX_TRAFFIC_LIGHT_INSTANCES). The manager stores one 8-byte
// LightState record per instance contiguously, with the array beginning at offset 0 of the
// manager (the X360 GetLightState returns `(u8*)this + 8 * luInstance`, i.e. the manager
// pointer IS the array base).
//
// Only ONE function is attested in the X360 retail XEX ledger for this TU:
//   GetLightState @ 0x8274F9A0  -- bounds-asserts the instance, returns &maLightStates[i].
// Its callers (ChangeLightState / TrafficLightGotSmashed / TrafficLightGotRestored) live in
// other TUs and are out of scope here.
//
// Layout pins (X360 asm, GetLightState @ 0x8274F9A0):
//   - record stride = 8 bytes:  slwi r11, instance, 3   (instance << 3 == * 8)
//   - record base   = this + 0: add  r3, r11, this      (no header before the array)
//   - bound = 0x258 (600):      cmplwi instance, 0x258  -> KU_MAX_TRAFFIC_LIGHT_INSTANCES
//
// LightState is an 8-byte record. The X360 GetLightState only computes &record; the two
// fields' names/types are not pinned by this TU's single attested function (the mutators
// that write the record are in other TUs). The 8-byte size is firm (the << 3 stride);
// the field decomposition below is an HONEST placeholder pending the mutator TUs.
// ---------------------------------------------------------------------------

namespace BrnTraffic
{
    // The flat instance-index bound asserted by GetLightState (X360: cmplwi ..., 0x258).
    static const u32 KU_MAX_TRAFFIC_LIGHT_INSTANCES = 0x258;  // 600

    // Per-traffic-light runtime state. Size pinned at 8 bytes by the GetLightState stride
    // (instance << 3). Field layout is a placeholder: the only attested function for this
    // TU just returns the record address; the writers that define the fields are in other
    // TUs (ChangeLightState / TrafficLightGotSmashed / TrafficLightGotRestored).
    struct TrafficLightState
    {
        u32 muStateA;  // PLACEHOLDER: 8-byte record, two 32-bit words; names unproven here.
        u32 muStateB;
    };

    class TrafficLightManager
    {
    public:
        // GetLightState @ 0x8274F9A0
        // Bounds-asserts the flat instance index, then returns the address of that
        // instance's 8-byte state record (the manager is the array base).
        //   asm: assert(instance < 0x258); return (u8*)this + 8 * instance;
        TrafficLightState* GetLightState(u32 luInstance);

    private:
        // The state array begins at offset 0 of the manager (record base == this).
        TrafficLightState maLightStates[KU_MAX_TRAFFIC_LIGHT_INSTANCES];
    };
}

#endif  // BRN_TRAFFIC_LIGHT_MANAGER_H
