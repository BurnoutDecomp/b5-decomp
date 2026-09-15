#ifndef BRN_SOUND_VEHICLES_ENGINES_PHYSICS_CONTROL_H
#define BRN_SOUND_VEHICLES_ENGINES_PHYSICS_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"   // committed BrnEffectControl dual base (BY NAME)
#include "GameSource/Sound/Vehicles/BrnVehicleState.h"              // VehicleState::EEngineComponentType / GetEngineComponentName / VehicleData (BY NAME)
#include "GameSource/AttribSys/Generated/classes/vehicleengine.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "BrnCommonTypes.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h" // Attribute::Key (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Engines::PhysicsControl
//   GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// PhysicsControl is the per-car physics->engine-audio bridge (DWARF
// BrnPhysicsControl.h:41: PhysicsControl : public BrnEffectControl). It caches the
// raw physics blob + a processed PhysicsData snapshot, plus the engine-rev intro
// block, and exposes engine-component key/name lookups back into VehicleState.
//
// FLAG (opaque-span layout): the two large embedded sub-objects the ctor constructs --
// mProcessedPhysicsData (PhysicsControl::PhysicsData, a big DataPoint/Average-of-
// Vector3/Matrix44 aggregate @ +0x40) and mVehicleEngineAttributes (Attrib::Gen::
// vehicleengine @ +0x228) -- have NO homed type in src. Per the anti-fabrication rule
// they are modelled as opaque byte spans (documented X360 sizes) rather than fabricated
// members; the intro-reving block is likewise a byte span with the one attested trailing
// flag byte. Named scalar members (mpVehicleState/mfOscillator/mpVehiclePhysicsData/...)
// are pinned BY NAME. Absolute offsets are NOT static_asserted across the 32/64 boundary.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
struct Car3DControl;
namespace Wheels { struct WheelControl; }
namespace Engines
{

// BrnPhysicsControl.h:41 (DWARF). Reuses the committed BrnEffectControl dual base BY NAME.
struct PhysicsControl : public BrnSound::Logic::BrnEffectControl
{
    struct PhysicsData
    {
        PhysicsData();

        CgsSound::Utils::DataPoint<bool> mIsAccelerating;
        bool mbJustShifted;
        CgsSound::Utils::DataPoint<s32> mGear;
        f32 mfDurationInGear;
        CgsSound::Utils::DataPoint<f32> mUnityRpm;
        CgsSound::Utils::DataPoint<f32> mNormalizedRpm;
        f32 mfMaxRpm;
        f32 mfIdleRpm;
        f32 mfTimeSinceRespawn;
        CgsSound::Utils::DataPoint<f32> mThrottle;
        CgsSound::Utils::Average<5u, f32> mDeltaThrottle;
        CgsSound::Utils::DataPoint<bool> IsBoosting;
        bool IsBlueBoost;
        f32 mfBoostRemaining;
        CgsSound::Utils::DataPoint<bool> IsCrashing;
        CgsSound::Utils::DataPoint<bool> IsDeforming;
        CgsSound::Utils::DataPoint<Vector3> mPosition3d;
        CgsSound::Utils::DataPoint<Vector2> mPosition2d;
        CgsSound::Utils::DataPoint<Vector3> mVelocity3d;
        CgsSound::Utils::DataPoint<Vector2> mVelocity2d;
        CgsSound::Utils::DataPoint<f32> mVelocityMagnitude;
        CgsSound::Utils::DataPoint<f32> mSpeedMPH;
        CgsSound::Utils::DataPoint<Vector3> mAcceleration3d;
        CgsSound::Utils::DataPoint<Vector2> mAcceleration2d;
        CgsSound::Utils::DataPoint<f32> mAccelerationMagnitude;
        CgsSound::Utils::DataPoint<Matrix44Affine> mTransform;
        CgsSound::Utils::DataPoint<f32> mYaw;
        CgsSound::Utils::DataPoint<f32> mSpeedMPS;
        f32 mfRotation;
        CgsSound::Utils::DataPoint<f32> mDrifting;
    };

    struct EngineRevEntry
    {
        EngineRevEntry() : mfTime(0.0f), mfRpm(0.0f), mfThrottle(0.0f) {}
        EngineRevEntry(f32 afTime, f32 afRpm, f32 afThrottle)
            : mfTime(afTime), mfRpm(afRpm), mfThrottle(afThrottle) {}
        f32 mfTime;
        f32 mfRpm;
        f32 mfThrottle;
    };

    struct EngRevDataSet
    {
        EngRevDataSet() : mnNumPoints(0), mpDataPoints(nullptr), mfTime(0.0f), mnCurrentPoint(0) {}
        // ADDITIVE: the four-field init the console's six static records at
        // unk_82F2F508 carry on disk (see BrnPhysicsControl.cpp).
        EngRevDataSet(s32 aiNumPoints, const EngineRevEntry* apPoints, f32 afTime, s32 aiCurrent)
            : mnNumPoints(aiNumPoints), mpDataPoints(apPoints)
            , mfTime(afTime), mnCurrentPoint(aiCurrent) {}
        s32 mnNumPoints;
        const EngineRevEntry* mpDataPoints;
        f32 mfTime;
        s32 mnCurrentPoint;
    };

    // The six authored start-line rev performances the draw @0x82684510 picks from
    // (the `% 6` of the LCG's high word).
    static const s32 KI_START_LINE_REV_SETS = 6;

    // BrnPhysicsControl.h:278 (DWARF). Intro-reving (start-line rev) sub-state.
    enum eIntroRevingState
    {
        E_NIS_REVING_STATE_OFF       = 0,
        E_NIS_REVING_STATE_STARTLINE = 1,
        E_NIS_REVING_STATE_RESUMING  = 2,
    };

    PhysicsControl();               // @ 0x826C8890
    virtual ~PhysicsControl();      // anchor for the vector deleting destructor @ 0x826AF8B0

    virtual s32 GetController(s32 aiSlot); // @ 0x82684388
    virtual void AttachController(CgsSound::Logic::EffectBase* apController); // @ 0x82684448
    virtual void SetupLoadData(); // @ 0x826E35C0
    virtual bool Attach(); // @ 0x826CB540
    virtual void UpdateParams(f32 afTimeStep); // @ 0x826CB710
    virtual void ProcessUpdate(); // @ 0x826E3B68

    // ------------------------------------------------------------------------
    // The PRIMARY vtable (off_820AF214, installed at this+0 by the ctor's
    // `stw r10, 0(r31)` @0x826C8900 -- off_820AF1E0 at this+4 is the EffectBase
    // sub-object's) carries three per-frame hooks, and UpdateParams @0x826CB710
    // calls all three through it at 0x826CC050..0x826CC064:
    //     vtable +8   0x826B2628  UpdateCollisionPassbys   (AI override 0x826B4DF8)
    //     vtable +12  0x8284CB38  an ICF-folded `blr` leaf in BOTH vtables
    //     vtable +16  0x826CC0C0  UpdateStartLineReving    (AI override 0x826CEC90)
    // The +12 slot is empty in the base AND in AIPhysicsControl (off_820AF4B4+12 is
    // the same folded `blr`), so it has no recoverable name and no observable effect;
    // it is deliberately NOT declared here rather than given an invented one.
    // ------------------------------------------------------------------------
    virtual void UpdateStartLineReving(f32 afTimeStep); // @ 0x826CC0C0

    // @ 0x82682CA8 (DWARF h:266). Forward to VehicleState::GetEngineComponentName.
    const char* GetEngineComponentName( BrnSound::Vehicles::VehicleState::EEngineComponentType aeComponentType );
    // @ 0x82682D10 (DWARF h:269). Read VehicleState's mEngineComponentKey[type].
    u64 GetEngineComponentKey( BrnSound::Vehicles::VehicleState::EEngineComponentType aeComponentType );
    // @ 0x82682DA0 (DWARF h:260). Return the cached raw physics blob.
    const BrnSound::Vehicles::VehicleData* GetRawPhysicsData() const;
    const PhysicsData& GetPhysicsData() const { return mProcessedPhysicsData; }
    const Attrib::Gen::vehicleengine& GetVehicleEngineAttributes() const { return mVehicleEngineAttributes; }
    BrnSound::Vehicles::VehicleState::AttachInfo GetAttachInfo() const { return mAttachInfo; }

    // @ 0x82684368 (DWARF h:218). Per-class static RTTI descriptor ("PhysicsControl").
    // 2-instruction leaf returning &sTypeInfo (function-local static ClassTypeInfo<
    // EffectControl>), the committed per-class GetStaticTypeInfo() accessor form
    // (ExplosionState::GetStaticTypeInfo precedent).
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetStaticTypeInfo();

    const BrnSound::Vehicles::VehicleData* mpVehiclePhysicsData;
    PhysicsData mProcessedPhysicsData;
    const BrnSound::Vehicles::Car3DControl* mp3dCarControl;
    const BrnSound::Vehicles::Wheels::WheelControl* mpWheelControl;
    Attrib::Gen::vehicleengine mVehicleEngineAttributes;
    BrnSound::Vehicles::VehicleState::AttachInfo mAttachInfo;
    CgsSound::Utils::DataPoint<f32> mfOscillator;
    CgsSound::Utils::DataPoint<f32> mfAngularVelocityAccumulator;
    eIntroRevingState meIntroRevingState;
    EngRevDataSet mEngineDataSet;
    CgsSound::Utils::InterpolateLine mEngineStartLineRPM;

    // [DIAG] NOT IN THE X360 BINARY -- BRN_ENGINE_DIAG witness accumulator.
    f32 mfDiagWitnessTimer;

protected:
    // BrnPhysicsControl.cpp:940, ARTIST @ 0x826B2860.  Converts the raw
    // physics RPM through the per-car cubic PhysicsRpmMap into [0,1].
    f32 UnityPhysicsRpm(f32 afPhysicsRPM) const;

    // The repeated speed-ramp DMix value of UpdateParams (ARTIST 0x826CBD20 and
    // its three copies): clamp(mph, 0, limit) * reciprocal * 32767.
    static s32 SpeedRampMixerValue(f32 afSpeedMPH, f32 afLimit, f32 afReciprocal);

    // @ 0x82684510 (IDA-truncated "BrnSound::Vehicles::Engines::Physic"). Step the
    // sound module's LCG and return the authored start-line rev performance the
    // draw lands on, one of the six at unk_82F2F508. Also inlined in Attach's tail.
    EngRevDataSet PickStartLineRevDataSet();

    // @ 0x826C8708 (IDA-truncated "BrnSound::Vehicles::Engines::PhysicsC"). Advance
    // a rev performance by afTimeStep and return the interpolated {time, rpm,
    // throttle} sample.
    static EngineRevEntry UpdateEngRevDataSet(EngRevDataSet& arSet, f32 afTimeStep);

    // [DIAG] NOT IN THE X360 BINARY -- BRN_ENGINE_DIAG (see BrnEngineAudioDiag.h).
    void EngineParamWitness(f32 afTimeStep);
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_PHYSICS_CONTROL_H
