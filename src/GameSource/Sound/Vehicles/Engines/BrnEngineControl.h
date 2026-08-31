#ifndef BRN_SOUND_VEHICLES_ENGINES_ENGINE_CONTROL_H
#define BRN_SOUND_VEHICLES_ENGINES_ENGINE_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"   // committed BrnEffectControl dual base (BY NAME)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameSource/Sound/Vehicles/Engines/BrnShiftControl.h"

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
// also carries the physics/shift/clutch/car3d/wheel control pointers, audio
// DataPoints, pitch/distortion ramps, redline state and shift LFO state. Those
// runtime paths are materialised here by their DWARF names and ARTIST behavior.
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
struct ClutchControl;

// Reuses the committed BrnEffectControl dual base BY NAME and the DWARF-attested
// ShiftControl::IShiftingActivator interface.
struct EngineControl : public BrnSound::Logic::BrnEffectControl,
                       public ShiftControl::IShiftingActivator
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
    virtual f32 GetStartRPM();
    virtual f32 GetTargetRPM();
    virtual f32 GetRiseFromRPM() { return mfAudioRpm.GetCurrent(); }
    const CgsSound::Utils::DataPoint<f32>& GetAudioRPM() const { return mfAudioRpm; }
    const CgsSound::Utils::DataPoint<f32>& GetAudioThrottle() const { return mfAudioThrottle; }
    const CgsSound::Utils::DataPoint<f32>& GetAudioEngVolume() const { return mfAudioEngineVolume; }
    f32 GetAudioPitch() const { return mAudioPitch.GetValueFloat(); }
    bool GetClutchState() const { return mbClutchStateOn; }

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
    bool ShouldTurnOnClutch(f32 afTargetRpm) const;
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_ENGINE_CONTROL_H
