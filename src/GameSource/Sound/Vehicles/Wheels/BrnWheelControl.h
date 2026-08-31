#ifndef BRN_SOUND_VEHICLES_WHEELS_WHEEL_CONTROL_H
#define BRN_SOUND_VEHICLES_WHEELS_WHEEL_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"   // committed BrnEffectControl dual base (BY NAME)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"

// =============================================================================
// BrnSound::Vehicles::Wheels::WheelControl  (+ leaf AIWheelControl)
//   GameSource/Sound/Vehicles/Wheels/BrnWheelControl.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// WheelControl is the per-car wheel/skid sound CONTROL. DWARF
// (BrnWheelControl.h:129): AIWheelControl : public WheelControl, and WheelControl
// derives from the committed BrnEffectControl (its X360 ctor installs the same
// dual-vptr pair -- primary EffectControl @ this+0, IResourceRequester sub-object
// @ this+4 -- plus a third IShiftingActivator sub-object vptr @ +0x38).
//
// This home still defers the road-noise/skid producer surface, but materialises the
// DWARF-named in-air modifiers consumed directly by EngineControl. Keeping that
// boundary typed prevents the engine selector from silently bypassing authored
// in-air RPM/throttle/volume data while the rest of WheelControl is recovered.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

// BrnWheelControl.h (DWARF). The wheel/skid sound control base. Reuses the committed
// BrnEffectControl dual base BY NAME.
struct WheelControl : public BrnSound::Logic::BrnEffectControl
{
    enum EInAirRevState
    {
        E_IN_AIR_REV_STATE_NONE = 0,
        E_IN_AIR_REV_STATE_ASCENDING = 1,
        E_IN_AIR_REV_STATE_DESCENDING = 2,
    };

    WheelControl()
        : meInAirRevState(E_IN_AIR_REV_STATE_NONE)
        , mfAudioRPM(0.0f)
        , mInAirRevThrottlePath()
        , mInAirRevRpmInterpolate()
        , mInAirRevVolumeInterpolate()
    {
    }
    virtual ~WheelControl();   // anchor for the vector deleting destructor @ 0x826D00A0

    bool IsActive() const { return meInAirRevState != E_IN_AIR_REV_STATE_NONE; }
    f32 GetAudioRPM() const { return mfAudioRPM; }
    f32 GetModifiedRpm() const { return mInAirRevRpmInterpolate.GetValueFloat(); }
    f32 GetModifiedThrottle() const { return mInAirRevThrottlePath.mfCurrentValue; }
    f32 GetModifiedVolume() const { return mInAirRevVolumeInterpolate.GetValueFloat(); }

private:
    EInAirRevState meInAirRevState;
    f32 mfAudioRPM;
    CgsSound::Utils::PathLine<3u> mInAirRevThrottlePath;
    CgsSound::Utils::InterpolateLine mInAirRevRpmInterpolate;
    CgsSound::Utils::InterpolateLine mInAirRevVolumeInterpolate;
};

// BrnWheelControl.h:129 (DWARF): AIWheelControl : public WheelControl. Adds no data
// members of its own; the three leaf vptr installs (primary/EffectControl @+0,
// IResourceRequester sub-object @+4, IShiftingActivator sub-object @+0x38) are
// produced structurally by the WheelControl base spine + the virtual ~AIWheelControl.
struct AIWheelControl : public WheelControl
{
    AIWheelControl();                 // @ 0x826E55A8
    virtual ~AIWheelControl();        // anchor for the vector deleting destructor @ 0x826E5608
};

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_WHEELS_WHEEL_CONTROL_H
