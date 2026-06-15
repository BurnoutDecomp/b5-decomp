#pragma once

// Vehicle-manager event payloads (the subset the boot-path event queues embed).
// Reconstructed from the DecFIGS DWARF (member names/types). SIMD-bearing events are
// 16-byte aligned; the rest take their natural alignment. (PhysicalTrafficState is
// reconstructed separately — it pulls in the Wheel/WheelLite/Vector4 cascade.)
#include "BrnCommonTypes.h"                                                           // Vector3, Matrix44Affine, EntityId, CgsID
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

    struct TrafficRemovedEvent
    {
        EntityId     mRemovedVehicleEntityId;
        ETrafficType meTrafficType;
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
}
}
