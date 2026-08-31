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
        s32 mnNumPoints;
        const EngineRevEntry* mpDataPoints;
        f32 mfTime;
        s32 mnCurrentPoint;
    };

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

protected:
    // BrnPhysicsControl.cpp:940, ARTIST @ 0x826B2860.  Converts the raw
    // physics RPM through the per-car cubic PhysicsRpmMap into [0,1].
    f32 UnityPhysicsRpm(f32 afPhysicsRPM) const;
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_PHYSICS_CONTROL_H
