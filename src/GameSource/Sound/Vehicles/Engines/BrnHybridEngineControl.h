#ifndef BRN_SOUND_VEHICLES_ENGINES_HYBRID_ENGINE_CONTROL_H
#define BRN_SOUND_VEHICLES_ENGINES_HYBRID_ENGINE_CONTROL_H

#include "types.hpp"

// =============================================================================
// BrnSound::Vehicles::Engines::HybridEngineControl
//   GameSource/Sound/Vehicles/Engines/BrnHybridEngineControl.h  (DWARF home) +
//   GameSource/Sound/Vehicles/Engines/BrnHybridEngineControl.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity; store/load widths
// authoritative). HybridEngineControl is the dual-source (loop + Ginsu) engine
// EffectControl; the DWARF shows the HybridExhaustControl base carrying the
// physics/engine/shift/clutch control pointers, the per-frame DataPoints
// (mPhysicsDeltaRpm/mAudioDeltaRpm/mGinsuRpm), the decel crossfade Graph and the
// EngineMix output accumulators.
//
// FLAG (MINIMAL home): this group reconstructs ONLY the one boot-trace ledger
// function of this header-id TU:
//   HybridEngineControl::UpdateGinsuRPM  @ 0x82699B98
// The full HybridEngineControl / HybridExhaustControl surface (RTTI, Attach,
// UpdateParams, UpdateDeltaRPM, UpdateMix and the volume update chain) is its own
// keystone TU and is DEFERRED. Only the two members UpdateGinsuRPM touches plus the
// source control pointer are modelled here, BY NAME.
//
// UpdateGinsuRPM is a single-step shift of the Ginsu RPM DataPoint: it samples the
// sampled engine control's current Ginsu RPM, pushes this control's current value
// into its previous slot, then stores the freshly sampled value as the new current.
// The X360 asm (@ 0x82699B98):
//   lwz  r11, 0x130(this)   ; mpEngineControl
//   lfs  f13, 0x8C(this)    ; mGinsuRpm.mfCurrent
//   lfs  f0,  0x8C(r11)     ; mpEngineControl->mGinsuRpm.mfCurrent (the new sample)
//   stfs f13, 0x90(this)    ; mGinsuRpm.mfPrevious = old current
//   stfs f0,  0x8C(this)    ; mGinsuRpm.mfCurrent  = new sample
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 asm uses the absolute byte
// offsets +0x130 (the sampled control pointer), +0x8C (mGinsuRpm current) and +0x90
// (mGinsuRpm previous). Those assume 4-byte pointers/vptr; on a 64-bit host pointer/
// vptr widths differ, so members are pinned BY NAME only and the absolute offsets are
// NOT static_asserted across pointers. The current/previous pair is the committed
// DataPoint<f32> shape (mfCurrent then mfPrevious).
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// MINIMAL DataPoint<f32> shape (full template in the DataPoint keystone TU). The
// X360 stfs writes the current value at +0 and the previous value at +4, so the
// shift in UpdateGinsuRPM is mfPrevious = mfCurrent; mfCurrent = sampled.
// FLAG: minimal — modelled only for the GinsuRpm shift this group bodies.
struct GinsuDataPoint
{
    f32 mfCurrent;  // +0x8C in HybridEngineControl
    f32 mfPrevious; // +0x90 in HybridEngineControl
};

// MINIMAL home (full layout in the EngineControl keystone TU). UpdateGinsuRPM only
// reads the sampled engine control's current Ginsu RPM (its DataPoint current).
// FLAG: minimal forward shape; the +0x8C placement mirrors HybridEngineControl's own
// mGinsuRpm slot in the X360 layout but is pinned BY NAME only.
struct GinsuSampleSource
{
    GinsuDataPoint mGinsuRpm; // the control's own current/previous Ginsu RPM pair
};

// MINIMAL home (full layout in the HybridEngineControl keystone TU). Only the Ginsu
// RPM DataPoint and the sampled control pointer that UpdateGinsuRPM touches are
// modelled BY NAME.
struct HybridEngineControl
{
    // @ 0x82699B98 — shift the Ginsu RPM DataPoint forward one frame, sampling the
    // freshly-computed Ginsu RPM from the engine control.
    void UpdateGinsuRPM();

    // The engine control this hybrid samples its Ginsu RPM from (X360 +0x130).
    // FLAG: byte offset not asserted on the 64-bit host; pinned by name only.
    GinsuSampleSource* mpEngineControl;

    // This control's own Ginsu RPM DataPoint (X360 mfCurrent @ +0x8C, mfPrevious
    // @ +0x90). FLAG: byte offsets not asserted on the 64-bit host.
    GinsuDataPoint mGinsuRpm;
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_HYBRID_ENGINE_CONTROL_H
