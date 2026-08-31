#ifndef BRN_SOUND_VEHICLES_ENGINES_ENGINE_CONTROL_H
#define BRN_SOUND_VEHICLES_ENGINES_ENGINE_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"   // committed BrnEffectControl dual base (BY NAME)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"

// =============================================================================
// BrnSound::Vehicles::Engines::EngineControl
//   GameSource/Sound/Vehicles/Engines/BrnEngineControl.h  (DWARF home) +
//   GameSource/Sound/Vehicles/Engines/BrnEngineControl.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity; store/load widths
// authoritative). EngineControl is the vehicle engine-sound EffectControl. The X360
// `vector deleting destructor' @ 0x826B2BA0 proves the SAME tri-base shape already
// reconstructed for the sibling ClutchControl / WheelControl homes: it installs the
// committed BrnEffectControl dual-vptr pair (primary EffectControl @ this+0,
// IResourceRequester sub-object @ this+4, progressively settled off_820AEA6C /
// off_820AEA38 -> off_820AA820 -- byte-identical to BrnEffectControl's own dtor @
// 0x826AEF68) PLUS a third ShiftControl::IShiftingActivator sub-object vptr
// (off_820AF228) written at this+0x38, matching DWARF's "derives from
// BrnSound::Vehicles::Engines::ShiftControl::IShiftingActivator" note. EngineControl
// also carries the physics/shift/clutch/car3d/wheel control pointers plus the audio
// DataPoints (mfAudioRpm/Throttle/EngineVolume), the interpolators (mAudioPitch/
// mAudioDistortion), the redline + distortion state machines and the LFO scratch --
// all DEFERRED (see FLAG).
//
// FLAG (MINIMAL home): this group reconstructs exactly TWO ledger functions:
//   EngineControl::GetStartRPM                    @ 0x82698FC8  (lfs f1,0x18(r3); blr)
//   EngineControl::`vector deleting destructor'    @ 0x826B2BA0  (-> ~EngineControl anchor)
// The full EngineControl surface (RTTI CreateObject/GetTypeInfo, the attach/
// detach/update pipeline UpdateRPM/Throttle/Volume/Pitch/Distortion/RedLiningRPM,
// the physics/shift/clutch/car3d/wheel control pointers, and the
// ShiftControl::IShiftingActivator interface's own members) is its own keystone TU
// and is DEFERRED, mirroring the committed ClutchControl / WheelControl minimal-home
// convention exactly: EngineControl inherits ONLY the committed BrnEffectControl base
// in code here; the third IShiftingActivator sub-object vptr write is documented as
// STRUCTURAL commentary (same as the ClutchControl / WheelControl homes) rather than
// modelled with a hand-declared base, since ShiftControl (and its nested
// IShiftingActivator) already has its own real header home (BrnShiftControl.h) that
// this minimal home must not re-fork/redefine. Only the single f32 field GetStartRPM
// reads and the dtor's inherited-BrnEffectControl-base teardown are modelled here, BY
// NAME.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 asm reads the start-RPM at the
// absolute byte offset +0x18 and installs the IShiftingActivator sub-object vptr at
// +0x38. On X360 those offsets assume 4-byte pointers/vptr; on a 64-bit host
// pointer/vptr widths differ, so members/bases are pinned BY NAME + SEQUENCE only
// and absolute offsets are NOT static_asserted across pointers.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
struct Car3DControl;
namespace Wheels { struct WheelControl; }
namespace Engines
{

struct PhysicsControl;
struct ShiftControl;
struct ClutchControl;

// MINIMAL home (full layout in the EngineControl keystone TU). Reuses the committed
// BrnEffectControl dual base BY NAME (mirrors ClutchControl / WheelControl). The DWARF-
// attested third base (ShiftControl::IShiftingActivator, real home BrnShiftControl.h)
// contributes only a structural sub-object vptr write at +0x38 in the X360 dtor; per the
// ClutchControl / WheelControl convention that is left as commentary, not a hand-declared
// base, to avoid re-forking ShiftControl's own header home. Only the start-RPM field
// GetStartRPM returns and the dtor anchor are reconstructed in this group.
struct EngineControl : public BrnSound::Logic::BrnEffectControl
{
    enum eRedlingState
    {
        E_REDLINING_STATE_OFF = 0,
        E_REDLINING_STATE_HIGH = 1,
        E_REDLINING_STATE_LOW = 2,
    };

    enum eDistortionState
    {
        E_DISTORTION_NONE = 0,
        E_DISTORTION_SLOW_ON = 1,
        E_DISTORTION_FAST_ON = 2,
        E_DISTORTION_INSTANT_ON = 3,
    };

    EngineControl();
    virtual ~EngineControl();   // anchor for the vector deleting destructor @ 0x826B2BA0

    virtual s32 GetController(s32 aiSlot); // @ 0x826845C0
    virtual void AttachController(CgsSound::Logic::EffectBase* apController); // @ 0x82684628
    virtual bool Attach(); // @ 0x82698FD0
    virtual void UpdateParams(f32 afTimeStep); // @ 0x826B2C58

    // @ 0x82698FC8 — return the cached engine start RPM (lfs f1, 0x18(this) on X360).
    f32 GetStartRPM() const;
    const CgsSound::Utils::DataPoint<f32>& GetAudioRPM() const { return mfAudioRpm; }
    const CgsSound::Utils::DataPoint<f32>& GetAudioThrottle() const { return mfAudioThrottle; }
    const CgsSound::Utils::DataPoint<f32>& GetAudioEngVolume() const { return mfAudioEngineVolume; }

    PhysicsControl* mpPhysicsControl;
    ShiftControl* mpShiftControl;
    ClutchControl* mpClutchControl;
    BrnSound::Vehicles::Car3DControl* mpCar3DControl;
    BrnSound::Vehicles::Wheels::WheelControl* mpWheelControl;
    CgsSound::Utils::DataPoint<f32> mfAudioRpm;
    CgsSound::Utils::DataPoint<f32> mfAudioThrottle;
    CgsSound::Utils::DataPoint<f32> mfAudioEngineVolume;
    CgsSound::Utils::InterpolateLine mAudioPitch;
    CgsSound::Utils::InterpolateLine mAudioDistortion;
    bool mbClutchStateOn;
    f32 mfVOL_LFO;
    f32 mfRPM_LFO;
    f32 mfAngleRPM_LFO;
    f32 mfAngleVOL_LFO;
    eRedlingState meRedLiningState;
    f32 mfRedlingRPMOffset;
    f32 mfRedlingVolumeScale;
    f32 mfRedliningTime;
    eDistortionState meDistortionState;
    f32 mfRPMRamping;

protected:
    virtual void UpdateRPM(f32 afTimeStep);
    virtual void UpdateThrottle(f32 afTimeStep);
    virtual void UpdateVolume(f32 afTimeStep);
    virtual void UpdateEnginePitch(f32 afTimeStep);
    virtual void UpdateDistortion(f32 afTimeStep);
    virtual void UpdateRedLiningRPM(f32 afTimeStep);
    virtual void UpdateEngineLFO(f32 afTimeStep);
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_ENGINE_CONTROL_H
