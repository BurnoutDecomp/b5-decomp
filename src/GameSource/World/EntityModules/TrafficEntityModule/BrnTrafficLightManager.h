#ifndef BRN_TRAFFIC_LIGHT_MANAGER_H
#define BRN_TRAFFIC_LIGHT_MANAGER_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
#include "BrnCommonTypes.h"                          // Vector3, VecFloat (the corona renderers, L3 2026-09-27)
#include "GameSource/Graphics/BrnCoronaManager.h"   // BrnCoronaManager::BrnSubmissionInterface (the same)

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

namespace BrnReplays { class TrafficEntitySerialiser; }

namespace BrnTraffic
{
    // Baked, read-only traffic-light corona view (home: SharedClasses/Traffic/Junctions/
    // BrnTrafficLightCollection.h). Referenced by-pointer only in TrafficLightGotSmashed;
    // forward-declared to avoid pulling the collection header into this manager header.
    class TrafficLightCollection;

    // ADDITIVE (crash parity FX-TRAFFICLIGHTS, 2026-09-25) -- UpdateHull's parameter. A struct: BrnTrafficHull.h
    // defines it as one and MSVC mangles the key.
    struct Hull;

    // The flat instance-index bound asserted by GetLightState (X360: cmplwi ..., 0x258).
    static const u32 KU_MAX_TRAFFIC_LIGHT_INSTANCES = 0x258;  // 600

    // Native light state: ARTIST uses 600 records of 8 bytes; DecFIGS names the type.
    struct TrafficLightRuntimeState
    {
        // Phase values asserted by Update (X360: byte at +4; >=3 fires "Unknown light state").
        enum E_State : u8
        {
            E_STATE_IDLE       = 0,  // no countdown running
            E_STATE_COUNTDOWN  = 1,  // mfTimeLeftInState ticking down toward the next phase
            E_STATE_HOLD       = 2,  // active hold; Update leaves it alone
            E_STATE_COUNT      = 3   // first invalid value (>=3 asserts)
        };

        f32 mfTimeLeftInState;     // +0
        u8  muState;     // +4
        u8  mxStateBits;     // +5  (low 3 bits carry the post-countdown sub-state)
        u8  maPad6[2];   // +6  (rounds the record to the 8-byte GetLightState stride)

        // Update @ 0x827515D8
        // Advance the light by lfTimeDelta. When counting down, subtract the delta from
        // mfTimeLeftInState; once it reaches <= 0, clamp the timer to 0, drop to E_STATE_IDLE, and
        // set the low sub-state bits of mxStateBits to 1. Unknown phases (>=3) assert.
        void Update(f32 lfTimeDelta);

        // ADDITIVE (L3 RACEINTRO, 2026-09-27) -- DWARF BrnTrafficLightManager.h :74 / :77, bodied in
        // BrnTrafficLightManager.cpp (DWARF .cpp:100 / :118). Both corona renderers inline them:
        //   IsSmashed      the flags byte's top bit: lbz +5 ; rlwinm 0,0,24 (& 0xFFFFFF80) ; cmplwi 0
        //                  (0x8275DD30..0x8275DD38, 0x8275DDEC..0x8275DDF4, 0x8275DF54..0x8275DF5C)
        //   GetLightState  the flags byte's low seven bits: the light's active-state mask, 1 << its state
        //                  (clrlwi r5, r11, 25 at 0x8275DE08)
        bool IsSmashed() const;
        u8   GetLightState() const;
    };

    // ARTIST copies a 0x12D0-byte manager and places the next member there.
    // Eight-byte alignment is the least sufficient XDK layout; original spelling/alignof is inferred.
#if defined(_XBOX)
    class CGS_ALIGN(8) TrafficLightManager
#else
    class TrafficLightManager
#endif
    {
        friend class BrnReplays::TrafficEntitySerialiser;
    public:
        // GetLightState @ 0x8274F9A0
        // Bounds-asserts the flat instance index, then returns the address of that
        // instance's 8-byte state record (the manager is the array base).
        //   asm: assert(instance < 0x258); return (u8*)this + 8 * instance;
        TrafficLightRuntimeState* GetLightState(u32 luInstance);

        // TrafficLightGotSmashed @ 0x827519A0 -- resolve the persistent instance id via the
        // baked collection's id hash and set the smashed bit (0x80) of that light's flags.
        void TrafficLightGotSmashed(const TrafficLightCollection* lpTrafficLightData,
                                    u32 luInstanceID);

        // TrafficLightGotRestored @ 0x82751A40 (40 insns) -- (cluster
        // `traffic`). The exact twin of TrafficLightGotSmashed with ONE instruction different:
        // `clrlwi r11,r11,25` (clear the top bit) where the smashed twin's tail is a single
        // `ori r11,r11,0x80` (0x82751A30, between `lbz r11,5(r31)` and `stb r11,5(r31)`) --
        // i.e. mxStateBits &= 0x7F instead of mxStateBits |= 0x80. There is NO `clrrwi` in the smashed
        // twin; the .cpp banner has always had this right. Assert strings + their
        // baked source lines: "lpTrafficLightData" :443 (0x1BB), "lpState" :451 (0x1C3).
        //
        // PROVENANCE NOTE (gotcha 6): this address has NO .ida-exports JSON and NO ledger row --
        // an exporter-run gap, not a missing function. The body was recovered by the wave-Q7
        // scout with headless idat on a PRIVATE copy of the .i64 (scratchpad/waveQ7/ida_tl/),
        // and is corroborated structurally by its already-verified smashed twin. Ask the
        // conductor to add the ledger row.
        void TrafficLightGotRestored(const TrafficLightCollection* lpTrafficLightData,
                                     u32 luInstanceID);

        // ADDITIVE (crash parity FX-NETCRASH, 2026-09-25) -- the event-countdown trio, DWARF
        // BrnTrafficLightManager.h :117 / :123 / :128, bodied in BrnTrafficLightManager.cpp:
        //   Construct         @ 0x82751708  every record GREEN with no time left; the countdown off
        //   SetCountdownValue @ 0x82751750  the event's countdown display drives every light
        //   Update            @ 0x827517A8  runs the countdown's GREEN phase down, then ends it
        void Construct();
        void SetCountdownValue(s32 liCountdownDisplay);
        void Update(f32 lfTimeDelta);

        // ADDITIVE (crash parity FX-NETCRASH, 2026-09-25) -- DWARF :160, bodied in BrnTrafficLightManager.cpp:
        //   ChangeLightState @ 0x827518E0  to red: a light not already RED goes AMBER for KF_AMBER_TIME;
        //                                  otherwise GREEN. Called by TrafficEntityModule::UpdateEventStarts.
        void ChangeLightState(u32 luInstance, bool lbChangeToRed);

        // ADDITIVE (crash parity FX-TRAFFICLIGHTS, 2026-09-25) -- DWARF :134, bodied in
        // BrnTrafficLightRuntimeState.cpp (this manager's .cpp cannot include BrnTrafficHull.h: BL-1):
        //   UpdateHull @ 0x827517F8  every light instance of the hull, [muFirstTrafficLight, muLastTrafficLight),
        //                            runs TrafficLightRuntimeState::Update: an AMBER light runs down to RED.
        //                            Called by TrafficEntityModule::UpdateJunctions for every active hull.
        void UpdateHull(const Hull* lpHull, f32 lfTimeDelta);

        // ADDITIVE (L3 RACEINTRO, 2026-09-27) -- the corona renderers, DWARF :144 / :154, bodied in
        // BrnTrafficLightManager.cpp (DWARF .cpp:290 / :353). TrafficEntityModule::RenderTrafficLightCoronas
        // @0x8271EC80 calls the first for every active hull and the second for every other hull in the camera
        // frustum:
        //   RenderLightsForHull               @0x8275DBF0  every unsmashed light of the hull in its own state -- or,
        //                                                  while an event counts down, in the countdown's state
        //   RenderAllLightsToBeInStateForHull @0x8275DE50  every unsmashed light of the hull in luActiveStates
        void RenderLightsForHull(const Hull* lpHull, const TrafficLightCollection* lpTrafficLightData,
                                 BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface,
                                 Vector3 lCameraPosition, Vector3 lCameraDirection, VecFloat lfCullDistSq) const;
        void RenderAllLightsToBeInStateForHull(const Hull* lpHull, const TrafficLightCollection* lpTrafficLightData,
                                               u32 luActiveStates,
                                               BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface,
                                               Vector3 lCameraPosition, Vector3 lCameraDirection,
                                               VecFloat lfCullDistSq) const;

    private:
        // The state array begins at offset 0 of the manager (record base == this).
        TrafficLightRuntimeState maLightStates[KU_MAX_TRAFFIC_LIGHT_INSTANCES];

        // ADDITIVE (crash parity FX-NETCRASH, 2026-09-25) -- DWARF :178..:180, the event-countdown
        // override. X360 +0x12C0 / +0x12C4 / +0x12C8: the three stores Construct makes after the
        // 600 records (0x82751740..0x82751748). TrafficLightManager::RenderLightsForHull @0x8275DBF0
        // reads the first two (lbz 0x12C0 / lwz 0x12C4) to draw every light of a hull in the
        // countdown's state.
        bool mbCountdownLights;          // :178  +0x12C0
        s32  meCountdownState;           // :179  +0x12C4  ETrafficLightState (RED 0 / AMBER 1 /
                                         //                GREEN 2 / COUNT 3), held as its 4-byte
                                         //                storage: the enum has two homes in this
                                         //                tree that cannot meet in one TU (BL-1)
        f32  mfCountdownRemainingTime;   // :180  +0x12C8

        // ADDITIVE (L3 RACEINTRO, 2026-09-27) -- DWARF :184, the const overload both corona renderers inline, bodied
        // at this header's line 217: the (0xD9) "luInstance < KU_MAX_TRAFFIC_LIGHT_INSTANCES" tripwire (cmplwi 0x258
        // at 0x8275DCF0 / 0x8275DDAC / 0x8275DF14), then this + 8 * luInstance.
        const TrafficLightRuntimeState* GetLightState(u32 luInstance) const
        {
            CGS_ASSERT(luInstance < KU_MAX_TRAFFIC_LIGHT_INSTANCES, "luInstance < KU_MAX_TRAFFIC_LIGHT_INSTANCES");
            return &maLightStates[luInstance];
        }
    };

#if defined(_XBOX)
    CGS_STATIC_ASSERT((sizeof(TrafficLightManager) == 0x12D0), "ARTIST traffic light manager extent");
#endif

}

#endif  // BRN_TRAFFIC_LIGHT_MANAGER_H
