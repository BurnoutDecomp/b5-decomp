#ifndef BRN_SOUND_VEHICLES_WHEELS_WHEEL_CONTROL_H
#define BRN_SOUND_VEHICLES_WHEELS_WHEEL_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"   // committed BrnEffectControl dual base (BY NAME)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameSource/Sound/Vehicles/Engines/BrnShiftControl.h"
#include "BrnCommonTypes.h"

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
struct RightSide3dControl;
struct LeftSide3dControl;
namespace Engines
{
struct PhysicsControl;
struct EngineControl;
}
namespace Wheels
{

// BrnWheelControl.h (DWARF). The wheel/skid sound control base. Reuses the committed
// BrnEffectControl dual base BY NAME.
struct WheelControl : public BrnSound::Logic::BrnEffectControl,
                      public BrnSound::Vehicles::Engines::ShiftControl::IShiftingActivator
{
    enum EWheelSide
    {
        E_LEFT_HAND_SIDE = 0,
        E_RIGHT_HAND_SIDE = 1,
        E_MAX_SIDES = 2,
    };

    enum EInAirRevState
    {
        E_IN_AIR_REV_STATE_NONE = 0,
        E_IN_AIR_REV_STATE_ASCENDING = 1,
        E_IN_AIR_REV_STATE_DESCENDING = 2,
        E_IN_AIR_REV_STATE_MERGING = 3,
    };

    struct WheelSide
    {
        WheelSide() { Construct(); }
        void Construct()
        {
            mbIsOnGround.Flush(false);
            mfPeel = mfBrake = mfLateral = 0.0f;
            mfPeelNormalized = mfBrakeNormalized = mfLateralNormalized = 0.0f;
            muRoadnoiseLoop = 0;
        }

        CgsSound::Utils::DataPoint<bool> mbIsOnGround;
        f32 mfPeel;
        f32 mfBrake;
        f32 mfLateral;
        f32 mfPeelNormalized;
        f32 mfBrakeNormalized;
        f32 mfLateralNormalized;
        s32 muRoadnoiseLoop;
    };

    struct WheelData
    {
        WheelData() { Construct(); }
        void Construct()
        {
            maSide[0].Construct();
            maSide[1].Construct();
            mfDrift.Flush(0.0f);
            mbReverse = false;
        }

        WheelSide maSide[E_MAX_SIDES];
        CgsSound::Utils::DataPoint<f32> mfDrift;
        bool mbReverse;
    };

    struct WheelAttribs
    {
        Vector2 mPeelSlow;
        Vector2 mPeelFast;
        Vector2 mLateralSlow;
        Vector2 mLateralFast;
        Vector2 mBrakeSlow;
        Vector2 mBrakeFast;
        f32 mafSideRightLateralMultipler[E_MAX_SIDES];
        f32 mafSideLeftLateralMultipler[E_MAX_SIDES];
        f32 mfSlowFastThreshold;
    };

    struct SingleWheelStatus
    {
        CgsSound::Utils::DataPoint<bool> mIsOnGround;
        CgsSound::Utils::DataPoint<u8> mSurfaceType;
    };

    WheelControl();
    virtual ~WheelControl();   // anchor for the vector deleting destructor @ 0x826D00A0

    virtual s32 GetController(s32 aiSlot) override;
    virtual void AttachController(CgsSound::Logic::EffectBase* apController) override;
    virtual bool Attach() override;
    virtual void UpdateParams(f32 afTimeStep) override;

    bool IsActive() const { return meInAirRevState != E_IN_AIR_REV_STATE_NONE; }
    const WheelData& GetWheelData() const { return mWheelData; }
    f32 GetAudioRPM() const { return mfAudioRPM; }
    CgsSound::Utils::DataPoint<bool> IsOnGround() const { return mIsOnGround; }
    SingleWheelStatus GetSingleWheelStatus(s32 aiWheelNumber) const
    {
        return mWheelStatus[aiWheelNumber];
    }
    f32 GetTimeInAir() const { return mfTimeInAir; }
    f32 GetTimeSinceLanding() const { return mfTimeSinceLanding; }
    f32 GetModifiedRpm() const { return mInAirRevRpmInterpolate.GetValueFloat(); }
    f32 GetModifiedThrottle() const { return mInAirRevThrottlePath.mfCurrentValue; }
    f32 GetModifiedVolume() const { return mInAirRevVolumeInterpolate.GetValueFloat(); }
    void SetRoadnoiseLoop(s32 aiLoop, u8 auSide)
    {
        mWheelData.maSide[auSide].muRoadnoiseLoop = aiLoop;
    }

    virtual f32 GetStartRPM() override;
    virtual f32 GetTargetRPM() override;
    virtual f32 GetRiseFromRPM() override;

private:
    void UpdateDriftingRPM();
    void UpdatePeelRPM(f32 afTimeStep);
    void UpdateWheelsInAirRPM(f32 afTimeStep);
    void UpdateWheelStatus(f32 afTimeStep);
    void UpdateSkidValues(f32 afTimeStep);
    f32 LerpedNormalise(const Vector2& arSlow, const Vector2& arFast,
                        f32 afValue, f32 afFraction) const;

    WheelAttribs mWheelAttribs;
    WheelData mWheelData;
    SingleWheelStatus mWheelStatus[4];
    BrnSound::Vehicles::Engines::PhysicsControl* mpPhysicsControl;
    BrnSound::Vehicles::Engines::ShiftControl* mpShiftControl;
    BrnSound::Vehicles::Engines::EngineControl* mpEngineControl;
    BrnSound::Vehicles::RightSide3dControl* mpRight3dControl;
    BrnSound::Vehicles::LeftSide3dControl* mpLeft3dControl;
    EInAirRevState meInAirRevState;
    CgsSound::Utils::DataPoint<bool> mIsOnGround;
    f32 mfAudioRPM;
    f32 mfRPMDueToDrift;
    f32 mfDriftingRPMFactor;
    f32 mfDriftingShiftOccured;
    bool mbPerformedDriftShift;
    bool mbIsDriftUpShift;
    f32 mfRPMDueToPeel;
    f32 mfPeelingRPMFactor;
    CgsSound::Utils::InterpolateLine mPeelOscillator;
    CgsNumeric::Random mRandomGenerator;
    CgsSound::Utils::PathLine<3u> mInAirRevThrottlePath;
    CgsSound::Utils::InterpolateLine mInAirRevRpmInterpolate;
    CgsSound::Utils::InterpolateLine mInAirRevVolumeInterpolate;
    f32 mfTimeInAir;
    f32 mfTimeSinceLanding;
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
