#pragma once

// ============================================================================
// BrnPhysics::Vehicle::VehicleInputInterface
//   GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h
//   (DWARF home BrnVehicleInputInterface.h:51)
//
// The per-frame request bundle the world/game-state modules push to the vehicle manager: the
// scene line-test results + triangle-cache interface, the create/remove/reset/validate race-car
// event queues, the traffic create/remove/crash event queues, the impact-event queue and the
// added-for-collision bit array. Reconstructed from BURNOUT_X360_ARTIST.XEX + the DecFIGS DWARF.
// Member names/types/order verbatim from the DWARF (BrnVehicleInputInterface.h:261..281); this
// is the real full member set (the previous NOMINAL 256-byte blob is replaced). Embedded BY VALUE
// in the RaceCarEntityModuleIO / WorldModuleIO buffers.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                          // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                        // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"          // CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"  // CgsSceneManager::SceneManagerIO::TriangleCacheInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"          // the create/remove/reset/traffic/impact event structs

namespace BrnPhysics
{
namespace Vehicle
{
    struct alignas(16) VehicleInputInterface
    {
        // ---- embedded-queue / interface typedefs (DWARF, homed via the includes above) --------
        typedef CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult, 2000> InLineTestResultQueue; // BrnPhysicsToSceneQueueIO.h:45
        typedef CgsSceneManager::SceneManagerIO::TriangleCacheInterface   InTriangleCacheInterface;   // BrnPhysicsToSceneQueueIO.h:48
        typedef CgsModule::EventQueue<CreateRaceCarEvent, 8>              CreateRaceCarEventQueue;               // :38
        typedef CgsModule::EventQueue<RemoveRaceCarEvent, 8>             RemoveRaceCarEventQueue;               // :39
        typedef CgsModule::EventQueue<ResetVehicleEvent, 16>            ResetRaceCarEventQueue;                // :52
        typedef CgsModule::EventQueue<ValidateRaceCarEvent, 8>          ValidateRaceCarEventQueue;             // :41
        typedef CgsModule::EventQueue<SetRaceCarCollisionEvent, 10>     SetRaceCarCollisionEventQueue;         // :42
        typedef CgsModule::EventQueue<SetRaceCarCullingGroupEvent, 10>  SetRaceCarCullingGroupEventQueue;      // :43
        typedef CgsModule::EventQueue<VehicleAddedForCollisionEvent, 64> NetworkCarsAddRemoveForCollisionQueue; // :50
        typedef CgsModule::EventQueue<CreatePhysicalTrafficEvent, 25>   CreateTrafficEventQueue;               // :44
        typedef CgsModule::EventQueue<CreateArticulatedTrafficEvent, 10> CreateArticulatedTrafficEventQueue;   // :46
        typedef CgsModule::EventQueue<SetTrafficCrashingEvent, 25>      SetTrafficCrashingEventQueue;          // :48
        typedef CgsModule::EventQueue<RemoveTrafficEvent, 25>           RemoveTrafficEventQueue;               // :47
        typedef CgsModule::EventQueue<UpdateNetworkTrafficEvent, 20>    UpdateNetworkTrafficEventQueue;        // :40
        typedef CgsModule::EventQueue<ImpactEvent, 16>                 ImpactEventQueue;                      // BrnVehicleEvents.h:575
        typedef CgsContainers::BitArray<8u>                            RaceCarBitArray;                       // :38

        // ---- wave-7 bodied ledger functions ---------------------------------------------------
        // @0x822CC1E8: enqueue a spawn-race-car request; returns the just-added slot index.
        s32  CreateRaceCar(VolumeInstanceId lVolumeInstanceId, Matrix44Affine lInitialTransform,
                           Vector3 lInitialVelocity, Vector3 lAngularVelocity,
                           Attribute::Key lCarAssetAttribKey, ResourceHandle lModelHandle,
                           ResourceHandle lGraphicsHandle, BrnWorld::ERaceCarType leRaceCarType,
                           f32 lfDeformAmount,
                           BrnPhysics::Deformation::DeformationResetType leBaseDeformationType,
                           bool lbDisablePhysicsStateReset, s32 liCarStrengthStat);

        // @0x822CC2A0: enqueue a reset-vehicle request.
        void ResetRaceCar(u32 luRaceCarIndex, Matrix44Affine lInitialTransform,
                          Vector3 lInitialVelocity, Vector3 lAngularVelocity, u8 lu8ResetTransform,
                          bool lbResetDeformation, bool lbResettingAfterWreck,
                          f32 lfRoadRageHowCloseToWrecked, bool lbResetTransform,
                          BrnPhysics::Deformation::DeformationResetType leDeformationResetType);

        // @0x8271D138 / @0x8271D1B8: enqueue a "traffic vehicle (not) crashing" event.
        void SetTrafficCrashing(EntityId lEntityId);
        void SetTrafficNotCrashing(EntityId lEntityId);

        // @0x822B4770: mark an active-race-car slot as added-for-collision.
        void SetRaceCarAddedForCollision(EActiveRaceCarIndex leRaceCarIndex);

        // Append the other interface's staged events onto this one (queue-merge).
        // ADDITIVE GROW: real X360 symbol (BrnPhysics::Vehicle::VehicleInputInterface::
        // Append, called by WorldModule::BridgeCrashModuleToPhysicsModule @0x827AACEC);
        // declaration-only (its own ledger function).
        void Append(const VehicleInputInterface& lrOther);

        // @0x82592FD0: hand-written copy assignment (Clear()+Append() per queue).
        VehicleInputInterface& operator=(const VehicleInputInterface& lrOther);

    private:
        InLineTestResultQueue                 mLineTestResultsQueue;                     // :261
        InTriangleCacheInterface              mTriangleCacheInterface;                   // :262
        CreateRaceCarEventQueue               mCreateRaceCarEventQueue;                  // :265
        RemoveRaceCarEventQueue               mRemoveRaceCarEventQueue;                  // :266
        ResetRaceCarEventQueue                mResetRaceCarEventQueue;                   // :267
        ValidateRaceCarEventQueue             mValidateRaceCarEventQueue;                // :268
        SetRaceCarCollisionEventQueue         mSetRaceCarCollisionEventQueue;            // :269
        SetRaceCarCullingGroupEventQueue      mSetRaceCarCullingGroupEventQueue;         // :270
        NetworkCarsAddRemoveForCollisionQueue mNetworkCarsAddedRemovedForCollisionQueue; // :271
        CreateTrafficEventQueue               mCreateTrafficEventQueue;                  // :274
        CreateArticulatedTrafficEventQueue    mCreateArticulatedTrafficEventQueue;       // :275
        SetTrafficCrashingEventQueue          mSetTrafficCrashingEventQueue;             // :276
        RemoveTrafficEventQueue               mRemoveCrashedTrafficEventQueue;           // :277
        UpdateNetworkTrafficEventQueue        mUpdateNetworkTrafficEventQueue;           // :278
        ImpactEventQueue                      mImpactEventQueue;                         // :280
        RaceCarBitArray                       mRaceCarsAddedForCollision;                // :281
    };
}
}
