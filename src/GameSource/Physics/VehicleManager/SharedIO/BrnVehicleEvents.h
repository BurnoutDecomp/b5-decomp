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
    //
    // ⭐⭐ THE "+4 DRIFT" IS SETTLED (2026-08-01, physics wave 1) -- and it was
    // mCarAssetAttribKey, which is EIGHT bytes, not four. Two earlier waves bounded the drift
    // without locating it (GameBridgeWorldToX.cpp's banner narrowed it to "between mHalfExtent
    // @848 and mEntityId @964"); the producer settles it outright.
    // BrnPhysics::Vehicle::VehicleOutputInterface::UpdateRaceCarState @0x825EC808 is the only
    // function that writes this struct, and it writes the key with a DOUBLEWORD store:
    //     lwz r11, 0x720(r30)      ; the physics body's mpAttribs
    //     ld  r11, 0x358(r11)      ; 8-byte load
    //     std r11, 0x3C0(r31)      ; 8-byte store at state + 960
    // Every one of the FOURTEEN scalar fields after it then lands 4 higher than the old model,
    // and each one corroborates the shift semantically (r31 == &maRaceCarStates[idx], r30 ==
    // the RaceCarPhysics):
    //     0x3CC=972  <- physics+0x6C0            (VehiclePhysics::mfSpeedMPH)     -> mfSpeedMPH
    //     0x3D0=976  <- mpAttribs+0x70  lane 2                                    -> mfMaxSpeedMPH
    //     0x3D4=980  <- mpAttribs+0x290 lane 1                                    -> mfMaxBoostSpeedMPH
    //     0x3D8=984  <- physics+0xFB0   lane 1                                    -> mfRPM
    //     0x3DC=988  <- mpAttribs[(gear+0x1D)*16] lane 2 (per-GEAR upshift rpm)   -> mfUpShiftRPM
    //     0x3E0=992  <- physics+0xF10   lane 3                                    -> mfDownShiftRPM
    //     0x3E4..0x3F8 = 996..1016 <- mpAttribs+0x1D0/0x1E0/0x1F0/0x200/0x210/0x220 lane 0
    //                                  (SIX consecutive gear ratios)              -> mafGearRatios[6]
    //     0x3FC=1020 <- physics+0x1000 lane 3 with the 0x80000000 sign bit cleared by `vandc`
    //                                  (an ABSOLUTE value of DriftScale)          -> mfAbsDriftScale
    //     0x400=1024 <- physics+0x1010 lane 2 (the header's "TimeDrifting" lane)  -> mfTimeDrifting
    //     0x408/0x40C/0x410 = 1032/1036/1040 <- driverControls+4/+8/+0xC          -> mfGas/Brake/HandBrake
    //     0x414=1044 <- the vtable slot-0 call's result (GetSteeringAngle)        -> mfSteering
    //     0x418=1048 <- physics+0x1040 lane 1 ("SideForceMag_TimeBoosting" .y)    -> mfTimeBoosting
    //     0x434=1076 <- physics+0xEF0 lane 1, else 0 when not crashing            -> mfTimeCrashing
    //     0x43C=1084 <- physics+0x13E0 (mi8SlammingRaceCarId) sign-extended        -> mi8LastAttackersRaceCarIndex
    //     0x444=1092 <- physics+0xFC0 (the same word used as the GEAR index above) -> mi8Gear
    //     0x44A/0x44B/0x44D = 1098/1099/1101 <- physics+0x710/+0x711/+0x712       -> mbCrashing/mbIsFatalyCrashing/mbStartedDeforming
    //     0x44C=1100 <- (physics+0x1436 == 0)                                     -> mbIsDriveable
    //     0x458=1112 <- driverControls+0xD0                                       -> meDriverType
    // ⇒ the member is spelled u64 here, NOT Attribute::Key. This tree typedefs
    // ::Attribute::Key to u32 (AttributeKey.h) and CgsAttribSysCollectionKey.cpp's own FLAG
    // says the honest width is 64 but declined to widen because "Attribute::Key is ALSO the
    // type of 4-byte serialised event payload fields (BrnVehicleEvents.h at @960 ...)".
    // That reason is now disproved for THIS field and for both create events below, so they
    // adopt u64 -- the same escape BrnRaceCarAIInterfaces.h:143 already took for
    // AttachAIControlEvent::mCarAssetAttribKey. The typedef itself is left alone (the
    // 4-byte serialised claim is untested for BrnDirectorEvents.h / BrnMessageData.h).
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
        u64                         mCarAssetAttribKey;              // @960 (8 bytes -- see banner)
        EntityId                    mEntityId;                       // @968
        f32                         mfSpeedMPH;                      // @972
        f32                         mfMaxSpeedMPH;                   // @976
        f32                         mfMaxBoostSpeedMPH;              // @980
        f32                         mfRPM;                           // @984
        f32                         mfUpShiftRPM;                    // @988
        f32                         mfDownShiftRPM;                  // @992
        f32                         mafGearRatios[6];                // @996
        f32                         mfAbsDriftScale;                 // @1020
        f32                         mfTimeDrifting;                  // @1024
        f32                         mfTimeInAir;                     // @1028
        f32                         mfGas;                           // @1032
        f32                         mfBrake;                         // @1036
        f32                         mfHandBrake;                     // @1040
        f32                         mfSteering;                      // @1044
        f32                         mfTimeBoosting;                  // @1048
        f32                         mfInProgressBarrelRollAngle;     // @1052
        f32                         mfInProgressAirSpinAngle;        // @1056
        f32                         mfInProgressHandbreakTurnAngle;  // @1060
        f32                         mfInProgressDriftTime;           // @1064
        f32                         mfInProgressDriftDistance;       // @1068
        f32                         mfTimeSinceLastRaceCarContact;   // @1072
        f32                         mfTimeCrashing;                  // @1076
        u32                         muStuntActionInProgress;         // @1080
        s32                         mi8LastAttackersRaceCarIndex;    // @1084 (int32 in DWARF)
        s32                         miRaceCarID;                     // @1088
        s8                          mi8Gear;                         // @1092
        s8                          mi8LastContactedRaceCar;         // @1093
        bool                        mabWheelExists[4];               // @1094
        bool                        mbCrashing;                      // @1098
        bool                        mbIsFatalyCrashing;              // @1099
        bool                        mbIsDriveable;                   // @1100
        bool                        mbStartedDeforming;              // @1101
        bool                        mbResetCarTransform;             // @1102
        bool                        mbJustBeenSlammed;               // @1103
        bool                        mbIsFrontRayOccluded;            // @1104
        bool                        mbIsWedgedInWorld;               // @1105
        bool                        mbIsHidden;                      // @1106
        bool                        mbContactingWall;                // @1107
        bool                        mbForceReset;                    // @1108
        bool                        mbDeformedThisFrame;             // @1109
        bool                        mbFullyDrivableFromCrash;        // @1110
        E_DRIVER_TYPE               meDriverType;                    // @1112

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
        // 8 BYTES (X360 event +0x70). operator= @0x825B7AE8 copies it with a single
        // `ld r11,0x70(r4) / std r11,0x70(r3)` pair and then copies mModelHandle as TWO
        // words (+0x78/+0x7C) before meTrafficType @0x80 / mbIsCab @0x84 / mCgsID @0x88
        // (`ld`). See RaceCarState's banner for why this is u64 and not Attribute::Key.
        // (The file's old "@112 / @120" annotation was derived from a 4-byte key and is wrong.)
        u64                mCarAssetAttribKey;
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
        // ⭐ 8 BYTES (X360 event +0x70), and this one is LOAD-BEARING TODAY. The event is
        // already posted every boot (ActiveRaceCar::AddHandlingModel ->
        // VehicleInputInterface::CreateRaceCar @0x822CC1E8) and its consumer,
        // VehicleManager::ProcessCreateEvents @0x82616770, does:
        //     ld  r11, 0x70(r3) ; std r11, <stack>        ; the key, 8 bytes
        //     ...
        //     lis/ori/insrdi r3 = 0x52B81656F3ADF675      ; == hash64("burnoutcarasset")
        //     ld  r4, <stack>                             ; the SAME 8 bytes
        //     bl  Attrib::FindCollection                  ; (classKey, collectionKey)
        // FindCollection hashes the WHOLE doubleword, so a 32-bit key MISSES -- the identical
        // defect the class key and Attrib::StringToKey each had (see BrnDirectorResourceManager.h).
        // Its producer chain is now u64 end to end: AttribSysCollectionKey::GetHashKey
        // @0x82805C20 -> VehicleListEntry::GetAttribCollectionKeyHash -> AddHandlingModel
        // @0x822D3EC8 -> here. operator= @0x822AE040 confirms the width (`ld/std @0x70`, then
        // mModelHandle as two words @0x78/0x7C, mGraphicsHandle two words @0x80/0x84,
        // meRaceCarType @0x88, mfDeformAmount @0x8C, meBaseDeformationType @0x90,
        // mbDisablePhysicsStateReset @0x94, miCarStrengthStat @0x98).
        u64                    mCarAssetAttribKey;
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
        // 8 BYTES each (X360 event +0xD0 / +0xD8). operator= @0x8270BF70 copies the run
        // 0xC0/0xC8/0xD0/0xD8 as FOUR `ld/std` doublewords -- the two VolumeInstanceIds then
        // these two keys -- and only then drops to word pairs for the two ResourceHandles
        // (+0xE0/+0xE8). They feed CreatePhysicalTrafficEvent::mCarAssetAttribKey verbatim
        // (CreateArticulatedTrafficEvent.cpp), which is itself 8 bytes; see RaceCarState's banner.
        u64 mAssetAttribKey_Cab;
        u64 mAssetAttribKey_Trailer;
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

        // X360 0x825B3030 / 0x825B3108: project the cab / trailer half of this articulated event
        // onto a CreatePhysicalTrafficEvent (bodied in CreateArticulatedTrafficEvent.cpp).
        void GetCreateCabEvent( CreatePhysicalTrafficEvent* lpCreateCabEvent ) const;
        void GetCreateTrailerEvent( CreatePhysicalTrafficEvent* lpCreateTrailerEvent ) const;
    };

    // Apply an air-ram impulse to a vehicle (jump/ram stunt effect). DWARF BrnVehicleEvents.h:586.
    // alignas(16) (carries Vector3Plus/Vector3). sizeof==64, matching the X360 EventQueue<...,20>
    // maEvents element stride (VolumeInstanceId(8)@0 + u32@8 + f32@12 + Vector3Plus(16)@16 +
    // Vector3(16)@32 + f32@48, rounded up to 64). Element of the VehicleEffectsInputInterface
    // mAirRamQueue.
    struct alignas(16) CreateAirRamEvent
    {
        VolumeInstanceId mVolumeId;              // @0
        u32              muEffectFlags;          // @8
        f32              mfDecay;                // @12
        Vector3Plus      mDirectionAndMagnitude; // @16
        Vector3          mPosition;              // @32
        f32              mfStartTime;            // @48
    };

    // Apply a spin impulse to a vehicle (air-spin stunt effect). DWARF BrnVehicleEvents.h:620.
    // alignas(16) (carries Vector3). sizeof==48 (VolumeInstanceId 8 -> Vector3 padded to @16 ->
    // f32 @32, rounded up to 48), matching the X360 EventQueue<...,10> / Append 48*count stride.
    // Element of the VehicleEffectsInputInterface mSpinQueue.
    struct alignas(16) CreateSpinEvent
    {
        VolumeInstanceId mVolumeId;  // @0
        Vector3          mForce;     // @16 (padded from 8; Vector3 is alignas(16))
        f32              mfTime;     // @32
    };

    // Result of a CreateVehicle request (which volume instance, success/fail). DWARF
    // BrnVehicleEvents.h:542. VolumeInstanceId is 8-byte aligned so the trailing bool pads out to
    // sizeof==16, matching the X360 EventQueue<...,8> / AddEvent / Append 16-byte element copy.
    // Element of the VehicleManagerOutputInterface mCreateVehicleResultQueue.
    struct CreateVehicleResult
    {
        VolumeInstanceId mVolumeInstanceID;  // @0
        bool             mbSuccess;          // @8
    };

    // Spawn/register a world-fixture vehicle at a fixed transform. DWARF BrnVehicleEvents.h:702.
    // alignas(16) (Matrix44Affine). Matrix44Affine(64)@0 + VolumeInstanceId(8)@64 == 72, rounded
    // up to sizeof==80. Element of the PhysicsModuleIO InputBuffer's mCreateWorldEventQueue.
    struct alignas(16) CreateWorldEvent
    {
        Matrix44Affine   mInitialTransform;   // @0
        VolumeInstanceId mVolumeInstanceId;   // @64
    };

    // Reset a race car to a fixed position after a wreck. DWARF BrnVehicleEvents.h:625.
    // alignas(16) (carries Vector3). EActiveRaceCarIndex(4)@0 + bool@4 + Vector3(16)@16 ->
    // sizeof==32, matching the X360 EventQueue<...,8> / AddEvent / Append 32-byte element stride
    // (the RaceCarResetEvent queue spanning +0x5B0..+0x6C0 in VehicleManagerOutputInterface).
    // Element of the VehicleManagerOutputInterface mRaceCarResetEventQueue.
    struct alignas(16) RaceCarResetEvent
    {
        EActiveRaceCarIndex meActiveRaceCarIndex;   // @0
        bool                mbResettingAfterWreck;  // @4
        Vector3             mResetPosition;         // @16 (padded from 5; Vector3 is alignas(16))
    };
}
}
