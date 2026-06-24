#ifndef BRN_SOUND_VEHICLES_ENGINES_ENGINE_CONTROL_H
#define BRN_SOUND_VEHICLES_ENGINES_ENGINE_CONTROL_H

#include "types.hpp"

// =============================================================================
// BrnSound::Vehicles::Engines::EngineControl
//   GameSource/Sound/Vehicles/Engines/BrnEngineControl.h  (DWARF home) +
//   GameSource/Sound/Vehicles/Engines/BrnEngineControl.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity; store/load widths
// authoritative). EngineControl is the vehicle engine-sound EffectControl; the
// DWARF shows it derives from BrnSound::Vehicles::Engines::ShiftControl::
// IShiftingActivator and carries the physics/shift/clutch/car3d/wheel control
// pointers plus the audio DataPoints (mfAudioRpm/Throttle/EngineVolume), the
// interpolators (mAudioPitch/mAudioDistortion), the redline + distortion state
// machines and the LFO scratch.
//
// FLAG (MINIMAL home): this group reconstructs ONLY the one boot-trace ledger
// function of this header-id TU:
//   EngineControl::GetStartRPM  @ 0x82698FC8   (lfs f1,0x18(r3); blr)
// The full EngineControl surface (RTTI CreateObject/GetTypeInfo, the attach/
// detach/update pipeline UpdateRPM/Throttle/Volume/Pitch/Distortion/RedLiningRPM
// and the ShiftControl::IShiftingActivator base + its DataPoint/InterpolateLine
// members) is its own keystone TU and is DEFERRED. Only the single f32 field the
// getter reads is modelled here, BY NAME.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 asm reads the start-RPM at the
// absolute byte offset +0x18. On X360 that offset follows the IShiftingActivator
// vptr/base sub-object plus the leading control pointers; on a 64-bit host pointer/
// vptr widths differ, so the field is pinned BY NAME only and the absolute +0x18
// offset is NOT static_asserted across pointers.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// MINIMAL home (full layout in the EngineControl keystone TU). Only the start-RPM
// field GetStartRPM returns is reconstructed in this group.
struct EngineControl
{
    // @ 0x82698FC8 — return the cached engine start RPM (lfs f1, 0x18(this)).
    f32 GetStartRPM() const;

    // The single f32 field GetStartRPM reads (X360 +0x18). FLAG: byte offset not
    // asserted on the 64-bit host; pinned by name only.
    f32 mfStartRPM;
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_ENGINE_CONTROL_H
