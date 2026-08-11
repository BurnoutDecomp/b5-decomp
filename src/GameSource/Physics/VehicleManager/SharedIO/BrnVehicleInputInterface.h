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
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT (SetRaceCarsAddedForCollision's tripwire)
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
                           u64 lCarAssetAttribKey, ResourceHandle lModelHandle,   // 64-bit: see BrnVehicleEvents.h
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

        // ⛔⛔ @0x822E66A0 -- ADDED 2026-08-01 (drivable wave) AND IT WAS A LIVE DEFECT.
        // This interface embeds FIFTEEN EventQueues by value, every one of which needs its
        // mpEvents pointed at its own inline storage. Nothing in the PC tree ever called
        // this: RaceCarEntityModuleIO::OutputBuffer_PrePhysics::Construct was a base-only
        // boot gate in WorldLinkStubs.cpp, and OutputBuffer_PreScene::Construct's partial
        // slice did not name it either. MEASURED the first time a car reached
        // ResetActiveRaceCar -> AddHandlingModel -> CreateRaceCar: the pair
        //   [ASSERT 1] mpEvents != NULL              (CgsBaseEventQueue.h:35)
        //   [ASSERT 2] EventQueue::AddEvent - Reached Max length  (:36)
        // followed by the process dying. Same family as the physics 1-byte game-action
        // queue and the 256-byte race-car queue of the previous two waves: invisible only
        // because nothing had ever posted into it.
        //
        // The console has TEN callers of this function; the two the PC tree can reach are
        // added with it.
        void Construct();

        // Append the other interface's staged events onto this one (queue-merge).
        // ADDITIVE GROW: real X360 symbol (BrnPhysics::Vehicle::VehicleInputInterface::
        // Append, called by WorldModule::BridgeCrashModuleToPhysicsModule @0x827AACEC);
        // declaration-only (its own ledger function).
        void Append(const VehicleInputInterface& lrOther);

        // @0x82592FD0: hand-written copy assignment (Clear()+Append() per queue).
        VehicleInputInterface& operator=(const VehicleInputInterface& lrOther);

        // Read access to the embedded triangle-cache interface (mTriangleCacheInterface, the second
        // member, at X360 byte +128016 == 16-byte InLineTestResultQueue header + 2000*64). The X360
        // inlines this as a raw `this + 128016` at its call sites (e.g. the crash-prediction driver
        // HandleCrashPredictionForRaceCarAndWorld). ADDITIVE inline accessor (header-only; no
        // out-of-line symbol), so host addressing stays layout-correct without the X360 byte offset.
        const InTriangleCacheInterface* GetTriangleCacheInterface() const { return &mTriangleCacheInterface; }

        // ⭐ ADDED 2026-08-06 (UpdateVehiclePhysics wave). Both accessors are DWARF-attested
        // (BrnVehicleInputInterface.h:186 GetLineTestResults / :216 GetImpactEventQueue); the
        // X360 inlines them as `this + 0` and `this + 141376` at the UpdateVehiclePhysics call
        // sites (asm 0x82645640 / 0x826452A0..B0). ADDITIVE header-only inlines -- host
        // addressing stays layout-correct without the console byte offsets.
        const InLineTestResultQueue* GetLineTestResults() const { return &mLineTestResultsQueue; }
        const ImpactEventQueue*      GetImpactEventQueue() const { return &mImpactEventQueue; }

        // ⭐ ADDED 2026-08-10 (create-path wave). Same ADDITIVE header-only inline as the two
        // above: the X360 reaches this queue as a raw `this + 128032` -- the pair
        // `addis r3,r4,2 ; addi r3,r3,-0xBE0` at the head of
        // VehicleManager::ProcessCreateEvents @0x82616770, which then reads the length at
        // +128040 (`lwz r11, 8(r3)`, i.e. BaseEventQueue::miLength) to bound its drain loop.
        // 128032 == 16 (InLineTestResultQueue header) + 2000*64 (its payload) + 16
        // (mTriangleCacheInterface) == the seat of mCreateRaceCarEventQueue, so the accessor is
        // the same member the console addresses, reached by name instead of by that offset.
        const CreateRaceCarEventQueue* GetCreateRaceCarEventQueue() const { return &mCreateRaceCarEventQueue; }

        // ⭐ ADDED 2026-08-11 (create-drain wave), the sibling of the accessor above and reached the
        // same way. VehicleManager::ProcessRemoveEvents @0x826160C8 opens with
        //     0x826160E4  addis r3, lpInputInterface, 2
        //     0x826160EC  addi  r3, r3, -0x6D0          -- 0x20000 - 0x6D0 == +130352
        //     0x826160F4  lwz   r11, 8(r3)              -- BaseEventQueue::miLength, the loop bound
        // and +130352 == 128032 (mCreateRaceCarEventQueue, pinned by the accessor above) + 2320
        // (that queue's own size) == the seat of mRemoveRaceCarEventQueue. ADDITIVE header-only
        // inline; the drain reaches the member by name instead of by that offset.
        const RemoveRaceCarEventQueue* GetRemoveRaceCarEventQueue() const { return &mRemoveRaceCarEventQueue; }

        // ⭐ ADDED 2026-08-10 (pre-physics bridge wave). BOTH ARE DWARF-DECLARED, not invented:
        // DecFIGS BrnVehicleInputInterface.h:245 `const RaceCarBitArray* GetRaceCarsAddedForCollision() const`
        // and :252 `void SetRaceCarsAddedForCollision(const RaceCarBitArray*)`. Neither has an
        // out-of-line X360 symbol; the pair is inlined at the tail of
        // WorldModule::BridgeEntityModulesToPhysicsModule_PrePhysics @0x827AAEC0:
        //   0x827AB2B0  ori  r31, r10, 0x2B50            -- 142160, the member's console seat
        //   0x827AB2C4  cmplwi r30, 0 / bne              -- the SETTER's own tripwire...
        //   0x827AB2CC  "lpRaceCarsAddedForCollision != NULL"  (X360 BrnVehicleInputInterface.h:254)
        //   0x827AB2EC  ld r11, 0(src) ; stdx r11, dst, r31   -- one 8-byte BitArray<8> copy
        // The console's assert sits INSIDE the setter (its parameter is what is tested), which is
        // why it fires on an address that can never be null -- reproduced as shipped.
        const RaceCarBitArray* GetRaceCarsAddedForCollision() const { return &mRaceCarsAddedForCollision; }
        void SetRaceCarsAddedForCollision(const RaceCarBitArray* lpRaceCarsAddedForCollision)
        {
            CGS_ASSERT(lpRaceCarsAddedForCollision != 0, "lpRaceCarsAddedForCollision != NULL");
            mRaceCarsAddedForCollision = *lpRaceCarsAddedForCollision;
        }

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
