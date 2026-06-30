#pragma once

// Vehicle-manager event payloads (the subset the boot-path event queues embed).
// Reconstructed from the DecFIGS DWARF (member names/types). SIMD-bearing events are
// 16-byte aligned; the rest take their natural alignment. (PhysicalTrafficState is
// reconstructed separately — it pulls in the Wheel/WheelLite/Vector4 cascade, which is
// now reconstructed below: WheelLite + RaceCarState live here, and their embedded types
// — Wheel::RoadContact, AboveGroundTestResult, VehiclePhysics::SlamEffect/ShuntEffect,
// E_DRIVER_TYPE — live in the headers included just below.)
#include "BrnCommonTypes.h"                                                           // Vector3, Matrix44Affine, EntityId, CgsID, CollisionTag
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"                   // BrnPhysics::Vehicle::Wheel::RoadContact
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h" // BrnPhysics::Vehicle::AboveGroundTestResult
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"          // VehiclePhysics::SlamEffect / ShuntEffect
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"      // BrnPhysics::Vehicle::E_DRIVER_TYPE
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"                  // CgsSceneManager::VolumeInstanceId
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"                 // CgsResource::ResourceHandle
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"    // Attribute::Key
#include "GameSource/BurnoutConstants.h"                                              // EActiveRaceCarIndex
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"                    // EImpactType, ETrafficType
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h"  // BrnWorld::ERaceCarType
#include "GameSource/GameState/BrnTakedownType.h"                                     // BrnGameState::ETakedownType
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"      // BrnPhysics::Deformation::DeformationResetType

namespace BrnPhysics
{
namespace Vehicle
{
    using CgsSceneManager::VolumeInstanceId;
    using CgsResource::ResourceHandle;

    // Per-wheel sim state published to consumers (a "lite" projection of Wheel). Embedded
    // 4x in RaceCarState (and PhysicalTrafficState). Layout/order/types from the DWARF
    // (BrnVehicleEvents.h:88). 16-byte aligned (carries Vector3 / RoadContact).
    struct alignas(16) WheelLite
    {
        Wheel::RoadContact mRoadContact;
        Vector3            mVelocity;
        f32                mfSuspensionHeight;
        f32                mfRadiansPerSecond;
        f32                mfRadius;
        f32                mfRotation;
        f32                mfSkidFactor;
        f32                mfWheelLongSpeed;
        f32                mfRoadLongSpeed;
        f32                mfRoadLatSpeed;
        bool               mbAttached;
        bool               mbHasTraction;

        // Owned by the WheelLite/VehicleManager TU -- declare only (no body).
        void operator=(const WheelLite&);
    };

    // Full per-race-car physics snapshot published each frame. Embedded by value as
    // `RaceCarState maRaceCarStates[8]` in RCEntityActiveRaceCarOutputInterface, so it must
    // be a COMPLETE, default-constructible, copyable type. Layout/order/types are verbatim
    // from the DWARF (BrnVehicleEvents.h:144); the reconstructed member offsets sum to
    // sizeof == 1120, matching the X360 `memset(this, 0, 1120)` in Clear(). 16-byte aligned.
    struct alignas(16) RaceCarState
    {
        WheelLite                   maWheels[4];                     // @0
        AboveGroundTestResult       mAboveGroundTestResult;          // @448
        Matrix44Affine              mTransform;                      // @496
        Matrix44Affine              maWheelTransforms[4];            // @560
        Vector3                     mLinearVelocity;                 // @816
        Vector3                     mAngularVelocity;                // @832
        Vector3                     mHalfExtent;                     // @848
        Vector3                     mComOffset;                      // @864
        VehiclePhysics::SlamEffect  mSlamEffect;                     // @880
        VehiclePhysics::ShuntEffect mShuntEffect;                    // @928
        Attribute::Key              mCarAssetAttribKey;              // @960
        EntityId                    mEntityId;                       // @964
        f32                         mfSpeedMPH;                      // @968
        f32                         mfMaxSpeedMPH;                   // @972
        f32                         mfMaxBoostSpeedMPH;              // @976
        f32                         mfRPM;                           // @980
        f32                         mfUpShiftRPM;                    // @984
        f32                         mfDownShiftRPM;                  // @988
        f32                         mafGearRatios[6];                // @992
        f32                         mfAbsDriftScale;                 // @1016
        f32                         mfTimeDrifting;                  // @1020
        f32                         mfTimeInAir;                     // @1024
        f32                         mfGas;                           // @1028
        f32                         mfBrake;                         // @1032
        f32                         mfHandBrake;                     // @1036
        f32                         mfSteering;                      // @1040
        f32                         mfTimeBoosting;                  // @1044
        f32                         mfInProgressBarrelRollAngle;     // @1048
        f32                         mfInProgressAirSpinAngle;        // @1052
        f32                         mfInProgressHandbreakTurnAngle;  // @1056
        f32                         mfInProgressDriftTime;           // @1060
        f32                         mfInProgressDriftDistance;       // @1064
        f32                         mfTimeSinceLastRaceCarContact;   // @1068
        f32                         mfTimeCrashing;                  // @1072
        u32                         muStuntActionInProgress;         // @1076
        s32                         mi8LastAttackersRaceCarIndex;    // @1080 (int32 in DWARF)
        s32                         miRaceCarID;                     // @1084
        s8                          mi8Gear;                         // @1088
        s8                          mi8LastContactedRaceCar;         // @1089
        bool                        mabWheelExists[4];               // @1090
        bool                        mbCrashing;                      // @1094
        bool                        mbIsFatalyCrashing;              // @1095
        bool                        mbIsDriveable;                   // @1096
        bool                        mbStartedDeforming;              // @1097
        bool                        mbResetCarTransform;             // @1098
        bool                        mbJustBeenSlammed;               // @1099
        bool                        mbIsFrontRayOccluded;            // @1100
        bool                        mbIsWedgedInWorld;               // @1101
        bool                        mbIsHidden;                      // @1102
        bool                        mbContactingWall;                // @1103
        bool                        mbForceReset;                    // @1104
        bool                        mbDeformedThisFrame;             // @1105
        bool                        mbFullyDrivableFromCrash;        // @1106
        E_DRIVER_TYPE               meDriverType;                    // @1108

        // The array `RaceCarState maRaceCarStates[8]` is default-constructed, but declaring
        // a copy ctor below would suppress the implicit default ctor -- so provide one. The
        // X360 default-constructed each element via Clear() (callers Attach/Construct call
        // Clear()), so route the default ctor through Clear() to reproduce that init (NOT
        // `= default`, since the X360 default state is the Clear() state: identity
        // transforms etc., not all-zero).
        RaceCarState() { Clear(); }

        // Copy ctor @0x8220A4C0: the X360 body is a pure bitwise copy of the whole object
        // (memcpy 448 + memcpy 48 + VMX 16-byte matrix copies + word loops). RaceCarState is
        // trivially copyable, so a defaulted copy ctor reproduces it exactly. Defined in the
        // .cpp via `= default` (kept out-of-line so this ledger func has a definition site).
        RaceCarState(const RaceCarState&);

        // Ledger func @0x8229FFC8 -- defined in the .cpp.
        void Clear();

        // operator= has no X360 address (not a ledger func) -- declare only; another TU /
        // the implicit definition owns the body. Returns void per the DWARF (:214).
        void operator=(const RaceCarState&);
    };

    // Per-frame physics snapshot of a physical-traffic vehicle, published to consumers
    // (e.g. BrnEffectsGlassManager::UpdateVehicleEffectPositions copy-constructs one).
    // Layout/order/types verbatim from the DWARF (BrnVehicleEvents.h:121..131); the
    // reconstructed member offsets match the X360 copy-ctor @0x82284EE8 exactly:
    //   memcpy 448 (the WheelLite[4] head) + VMX 16-byte block copies across the
    //   matrices/vectors region [448..800) + scalar tail at +800 (mEntityID, u32),
    //   +804 (mfSpeed, f32), +808/+809/+810 (3 bools), +812 (mfSteering, f32).
    // sizeof == 816 (0x330). 16-byte aligned (carries Matrix44Affine / Vector3Plus).
    struct alignas(16) PhysicalTrafficState
    {
        WheelLite      maWheels[4];                       // @0   (4 * 112 = 448)
        Matrix44Affine mTransform;                        // @448
        Vector3Plus    mvRoadTestNormal_HeightAboveRoad;  // @512
        Vector3        mLinearVelocity;                   // @528
        Matrix44Affine maWheelTransforms[4];              // @544 (4 * 64 = 256)
        EntityId       mEntityID;                         // @800
        f32            mfSpeed;                           // @804
        bool           mbFrozen;                          // @808
        bool           mbIsDeforming;                     // @809
        bool           mbIsFatallyCrashing;               // @810
        f32            mfSteering;                         // @812

        // Copy ctor @0x82284EE8: the X360 body is a pure bitwise copy of the whole object
        // (memcpy 448 + VMX 16-byte block copies + scalar tail). PhysicalTrafficState is
        // trivially copyable, so a defaulted copy ctor reproduces it exactly. Kept
        // out-of-line in the .cpp via `= default` so this ledger func has a definition site.
        PhysicalTrafficState() {}
        PhysicalTrafficState( const PhysicalTrafficState& );
    };

    // A vehicle-vs-vehicle impact (aggressor/victim, severity, recovery).
    struct alignas(16) ImpactEvent
    {
        Vector3            mDirection;
        EImpactType        meImpactType;
        EActiveRaceCarIndex meAggressorActiveRaceCarIndex;
        EActiveRaceCarIndex meVictimActiveRaceCarIndex;
        f32                mfMagnitude;
        f32                mfDuration;
        f32                mfSteeringDirection;
        f32                mfRecoveryTime;
        u8                 muScore;
    };

    // Spawn a physical traffic vehicle.
    struct alignas(16) CreatePhysicalTrafficEvent
    {
        VolumeInstanceId   mVolumeInstanceID;
        EntityId           mCrasherID;
        Matrix44Affine     mInitialTransform;
        Vector3            mInitialVelocity;
        Vector3            mAngularVelocity;
        Attribute::Key     mCarAssetAttribKey;
        ResourceHandle     mModelHandle;
        ETrafficType       meTrafficType;
        bool               mbIsCab;
        CgsID              mCgsID;

        // Copy assignment @0x825B7AE8: the X360 body is a pure bitwise copy of the whole object
        // (ld/std scalar head + VMX 16-byte block copies across the transform/velocity region +
        // scalar tail @0x70/0x80/0x84/0x88, last store at +136). CreatePhysicalTrafficEvent is
        // trivially copyable, so a defaulted copy-assignment reproduces it exactly; kept
        // out-of-line in the .cpp so this ledger func has a definition site. ADDITIVE GROW
        // (flagged): an explicitly-declared-and-defaulted operator= over the same trivial copy.
        CreatePhysicalTrafficEvent& operator=( const CreatePhysicalTrafficEvent& );
        CreatePhysicalTrafficEvent() = default;
        CreatePhysicalTrafficEvent( const CreatePhysicalTrafficEvent& ) = default;
    };

    // Spawn a race car (player/AI/network).
    struct alignas(16) CreateRaceCarEvent
    {
        VolumeInstanceId       mVolumeInstanceID;
        Matrix44Affine         mInitialTransform;
        Vector3                mInitialVelocity;
        Vector3                mAngularVelocity;
        Attribute::Key         mCarAssetAttribKey;
        ResourceHandle         mModelHandle;
        ResourceHandle         mGraphicsHandle;
        BrnWorld::ERaceCarType meRaceCarType;
        f32                    mfDeformAmount;
        BrnPhysics::Deformation::DeformationResetType meBaseDeformationType;
        bool                   mbDisablePhysicsStateReset;
        s32                    miCarStrengthStat;

        // operator= @0x822AE040: a pure bitwise, field-for-field copy of the whole event (the
        // X360 body is an 8-byte head copy + four 16-byte VMX matrix-row copies + two more
        // 16-byte vector copies + an 8-byte + word/float/byte/word scalar tail -- i.e. a flat
        // memberwise copy of every field in declaration order). CreateRaceCarEvent is trivially
        // copyable, so a defaulted assignment reproduces it exactly; kept out-of-line (bodied in
        // the CreateRaceCarEvent ledger TU) so this ledger func has a definition site. Declared
        // only here (ADDITIVE GROW -- no field reordered/retyped).
        CreateRaceCarEvent& operator=(const CreateRaceCarEvent&);
    };

    struct RemoveRaceCarEvent
    {
        VolumeInstanceId mVolumeInstanceID;
    };

    struct RemoveTrafficEvent
    {
        VolumeInstanceId mVolumeInstanceID;
    };

    // Reset a vehicle's transform/velocity/deformation (e.g. after a wreck).
    struct alignas(16) ResetVehicleEvent
    {
        u32            miRaceCarIndex;
        Matrix44Affine mInitialTransform;
        Vector3        mInitialVelocity;
        Vector3        mAngularVelocity;
        bool           mbResetTransform;
        bool           mbResetDeformation;
        bool           mbResettingAfterWreck;
        f32            mfRoadRageHowCloseToWrecked;
        BrnPhysics::Deformation::DeformationResetType meDeformationResetType;
    };

    struct SetRaceCarCollisionEvent
    {
        EntityId mBodyId;
        bool     mbCollide;
    };

    struct SetRaceCarCullingGroupEvent
    {
        EntityId      mBodyId;
        typedef u32   CullingGroup;   // CgsSceneManagerTypes.h
        CullingGroup  mCullingGroup;
    };

    struct SetTrafficCrashingEvent
    {
        EntityId mEntityId;
        bool     mbCrashing;
    };

    // A physical-traffic vehicle was slammed/checked by another vehicle. Layout/order/types
    // verbatim from the DWARF (BrnVehicleEvents.h:493). Five 4-byte fields, natural
    // alignment => sizeof == 20 (0x14), matching the X360 BaseEventQueue<TrafficSlammedEvent>
    // AddEvent's 5-_DWORD element copy and Append's 20*count XMemCpy.
    // ADDITIVE GROW (flagged by Vehicle-events group): new event struct, no change to
    // existing types.
    struct TrafficSlammedEvent
    {
        EntityId          mTrafficId;
        EntityId          mEntityThatSlammedIt;
        eCrashTrafficType meCrashTrafficType;
        f32               mfSteeringDirection;
        f32               mfDriveDirection;
    };

    struct TrafficRemovedEvent
    {
        EntityId     mRemovedVehicleEntityId;
        ETrafficType meTrafficType;
    };

    // A physical-traffic vehicle crashed. Layout/order/types verbatim from the DWARF
    // (BrnVehicleEvents.h:478): VolumeInstanceId(8, 8-byte aligned) + EntityId(4). The
    // 8-byte alignment of VolumeInstanceId pads the trailing EntityId out to sizeof == 16
    // (0x10), matching the X360 BaseEventQueue<TrafficCrashedEvent>::AddEvent's two-_QWORD
    // (16-byte) element copy and Append's 16*count XMemCpy.
    // ADDITIVE GROW (flagged by Vehicle-events group): new event struct, no change to
    // existing types.
    struct TrafficCrashedEvent
    {
        VolumeInstanceId mTrafficVolumeInstanceID;
        EntityId         mCrasherEntityID;
    };

    struct alignas(16) UpdateNetworkTrafficEvent
    {
        VolumeInstanceId mVolumeInstanceID;
        Matrix44Affine   mTransform;
    };

    struct ValidateRaceCarEvent
    {
        bool             mbValidate;
        VolumeInstanceId mVolumeInstanceID;
        ResourceHandle   mModelHandle;
        ResourceHandle   mGraphicsHandle;
    };

    // A race car crashed (takedown/contact info).
    struct alignas(16) RaceCarCrashEvent
    {
        VolumeInstanceId            mRaceCarVolumeInstanceID;
        EntityId                    mCrasherEntityID;
        Vector3                     mCollisionNormal;
        Vector3                     mContactPoint;
        BrnGameState::ETakedownType meInstantTakedownType;
        f32                         mfSpeedMPH;
        bool                        mbIsPrimaryCrash;
        bool                        mbRemoveHandlingVolumeFromScene;
        bool                        mbCarIsAI;
        bool                        mbCarIsNetwork;
    };

    struct VehicleAddedForCollisionEvent
    {
        VolumeInstanceId mRaceCarVolumeInstanceId;
        bool             mbAdded;
    };

    // Spawn an articulated (cab + trailer) traffic vehicle. The cab/trailer halves
    // are split into two CreatePhysicalTrafficEvents downstream.
    struct alignas(16) CreateArticulatedTrafficEvent
    {
        Matrix44Affine mInitialTransform_Cab;
        Matrix44Affine mInitialTransform_Trailer;
        Vector3        mInitialVelocity_Cab;
        Vector3        mInitialVelocity_Trailer;
        Vector3        mAngularVelocity_Cab;
        Vector3        mAngularVelocity_Trailer;
        VolumeInstanceId mVolumeInstanceID_Cab;
        VolumeInstanceId mVolumeInstanceID_Trailer;
        Attribute::Key mAssetAttribKey_Cab;
        Attribute::Key mAssetAttribKey_Trailer;
        ResourceHandle mModelHandle_Cab;
        ResourceHandle mModelHandle_Trailer;
        CgsID          mCgsId_Cab;
        CgsID          mCgsId_Trailer;
        ETrafficType   meTrafficType;

        // Copy assignment @0x8270BF70: the X360 body is a pure bitwise copy of the whole object --
        // VMX 16-byte block copies across the cab/trailer matrices+vectors region [0..0xC0), then
        // ld/std + lwz/stw scalar copies of the VolumeInstanceId/AttribKey/ResourceHandle/CgsID
        // tail (last store at +256). CreateArticulatedTrafficEvent is trivially copyable, so a
        // defaulted copy-assignment reproduces it exactly; kept out-of-line in the .cpp so this
        // ledger func has a definition site. ADDITIVE GROW (flagged): an explicitly-declared-and-
        // defaulted operator= over the same trivial copy.
        CreateArticulatedTrafficEvent& operator=( const CreateArticulatedTrafficEvent& );
        CreateArticulatedTrafficEvent() = default;
        CreateArticulatedTrafficEvent( const CreateArticulatedTrafficEvent& ) = default;
    };
}
}
