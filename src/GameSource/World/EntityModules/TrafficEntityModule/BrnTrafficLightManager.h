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
    // Baked, read-only traffic-light corona view (home: SharedClasses/Traffic/Junctions/
    // BrnTrafficLightCollection.h). Referenced by-pointer only in TrafficLightGotSmashed;
    // forward-declared to avoid pulling the collection header into this manager header.
    class TrafficLightCollection;

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

    // -------------------------------------------------------------------------
    // BrnTraffic::TrafficLightRuntimeState  -- the per-traffic-light runtime record.
    //
    // ADDITIVE GROW (flagged by the brn-traffic2 bodies group). This is the real,
    // field-attested 8-byte record that TrafficLightRuntimeState::Update mutates
    // (X360 Update @ 0x827515D8). Its fields are now pinned by the asm:
    //   +0  f32 mfTimer       (lfs/fsubs/stfs at 0(this))   -- countdown seconds
    //   +4  u8  muState       (lbz/stb at 4(this))          -- light phase
    //   +5  u8  muFlags       (lbz/stb at 5(this))          -- low 3 bits = sub-state
    //   +6  u8  maPad6[2]     (8-byte record; +6/+7 untouched by Update)
    //
    // FLAG (committed-type convergence, NOT applied here): this is the same 8-byte
    // record TrafficLightManager::maLightStates[] holds (GetLightState's << 3 stride);
    // the older `TrafficLightState` above is the honest placeholder this supersedes.
    // Converging maLightStates' element type from TrafficLightState to
    // TrafficLightRuntimeState is a non-additive retype of a committed member, so it
    // is left to the maintainer (see flagged_type_changes); the placeholder is kept
    // untouched so this slice stays additive.
    struct TrafficLightRuntimeState
    {
        // Phase values asserted by Update (X360: byte at +4; >=3 fires "Unknown light state").
        enum E_State : u8
        {
            E_STATE_IDLE       = 0,  // no countdown running
            E_STATE_COUNTDOWN  = 1,  // mfTimer ticking down toward the next phase
            E_STATE_HOLD       = 2,  // active hold; Update leaves it alone
            E_STATE_COUNT      = 3   // first invalid value (>=3 asserts)
        };

        f32 mfTimer;     // +0
        u8  muState;     // +4
        u8  muFlags;     // +5  (low 3 bits carry the post-countdown sub-state)
        u8  maPad6[2];   // +6  (rounds the record to the 8-byte GetLightState stride)

        // Update @ 0x827515D8
        // Advance the light by lfTimeDelta. When counting down, subtract the delta from
        // mfTimer; once it reaches <= 0, clamp the timer to 0, drop to E_STATE_IDLE, and
        // set the low sub-state bits of muFlags to 1. Unknown phases (>=3) assert.
        void Update(f32 lfTimeDelta);
    };

    class TrafficLightManager
    {
    public:
        // GetLightState @ 0x8274F9A0
        // Bounds-asserts the flat instance index, then returns the address of that
        // instance's 8-byte state record (the manager is the array base).
        //   asm: assert(instance < 0x258); return (u8*)this + 8 * instance;
        TrafficLightState* GetLightState(u32 luInstance);

        // TrafficLightGotSmashed @ 0x827519A0 -- resolve the persistent instance id via the
        // baked collection's id hash and set the smashed bit (0x80) of that light's flags.
        void TrafficLightGotSmashed(const TrafficLightCollection* lpTrafficLightData,
                                    u32 luInstanceID);

    private:
        // The state array begins at offset 0 of the manager (record base == this).
        TrafficLightState maLightStates[KU_MAX_TRAFFIC_LIGHT_INSTANCES];
    };
}

#endif  // BRN_TRAFFIC_LIGHT_MANAGER_H
