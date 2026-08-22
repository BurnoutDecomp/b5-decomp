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

        // @ INLINED (no standalone X360 symbol; body recovered from its only caller,
        // ActiveRaceCar::RemoveHandlingModel @0x822D4070). DWARF
        // BrnVehicleInputInterface.h:126 `int32_t RemoveRaceCar(VolumeInstanceId)`.
        // Enqueue a destroy-race-car request for a body previously created by CreateRaceCar.
        // Header-only inline, same shape as AddLineTestResult above -- it emits no out-of-line
        // symbol, so nothing new reaches the link.
        // [FLAG] the returned value is AddEvent's success flag widened to the DWARF's int32_t;
        // nothing reads it (the console's caller tail-returns r3 and discards it).
        s32 RemoveRaceCar(VolumeInstanceId lVolumeInstanceId)
        {
            RemoveRaceCarEvent lEvent;
            lEvent.mVolumeInstanceID = lVolumeInstanceId;
            return mRemoveRaceCarEventQueue.AddEvent(lEvent) ? 1 : 0;
        }


        // ---- wave-T3 (PHYSICAL TRAFFIC) producers ---------------------------------------------
        // @0x8271C600 (240). DWARF BrnVehicleInputInterface.h:141. THE world->physics spawn hop
        // for a promoted traffic car: fills a CreatePhysicalTrafficEvent and appends it to
        // mCreateTrafficEventQueue (console this+132976). Four baked asserts at the console's own
        // header lines :483/:484/:485/:486. The DWARF return is void; the console leaves
        // AddEvent's result in r3 and its one caller (TrafficEntityModule::AddVehicleToPhysics
        // @0x827427C0) does not read it.
        void CreatePhysicalTraffic(VolumeInstanceId lVolumeInstanceId, EntityId lCrasherId,
                                   Matrix44Affine lInitialTransform,
                                   Vector3 lInitialVelocity, Vector3 lAngularVelocity,
                                   u64 lCarAssetAttribKey,       // Attribute::Key -- 8 bytes
                                   ResourceHandle lModelHandle,
                                   ETrafficType leTrafficType, bool lbIsCab, CgsID lCgsID);

        // THE DEMOTION TWIN of CreatePhysicalTraffic (wave T3 round 3). The console inlines
        // this at its single call site -- TrafficEntityModule::CleanUpCrashedVehiclePhysics
        // @0x82720A6C `bl RemoveTrafficEvent::AddEvent` on the interface + 0x22118, which is
        // mRemoveCrashedTrafficEventQueue -- so it emits no out-of-line symbol, exactly like
        // RemoveRaceCar above. PhysicalTrafficManager::ProcessRemoveEvents drains it and
        // frees the physical slot.
        void RemovePhysicalTraffic(VolumeInstanceId lVolumeInstanceId)
        {
            RemoveTrafficEvent lEvent;
            lEvent.mVolumeInstanceID = lVolumeInstanceId;
            mRemoveCrashedTrafficEventQueue.AddEvent(lEvent);
        }

        // @0x8271C9C0 (478). DWARF :144. The trailer (cab + trailer) twin. DECLARED so
        // AddVehicleToPhysics' articulated arm compiles; the body is a named gate (trailers are
        // parked for wave-T3 round 1 -- wave-T2 generation only builds InitialiseAsStandard cars,
        // so the arm is unreachable today).
        void CreateArticulatedTraffic(VolumeInstanceId lVolumeInstanceId_Cab,
                                      Matrix44Affine lInitialTransform_Cab,
                                      Vector3 lInitialVelocity_Cab, Vector3 lAngularVelocity_Cab,
                                      u64 lAssetAttribKey_Cab, ResourceHandle lModelHandle_Cab,
                                      CgsID lCgsId_Cab,
                                      VolumeInstanceId lVolumeInstanceId_Trailer,
                                      Matrix44Affine lInitialTransform_Trailer,
                                      Vector3 lInitialVelocity_Trailer,
                                      Vector3 lAngularVelocity_Trailer,
                                      u64 lAssetAttribKey_Trailer,
                                      ResourceHandle lModelHandle_Trailer,
                                      CgsID lCgsId_Trailer,
                                      ETrafficType leTrafficType);

        // @0x822CC2A0: enqueue a reset-vehicle request.
        // ⛔ [teleport] PARAMETER NAMES CORRECTED 2026-08-21 -- the LEADING u8 is the unused slot,
        // not the trailing bool, and the three bools are ResetTransform / ResetDeformation /
        // ResettingAfterWreck in THAT order with the f32 between the last two. Full asm derivation
        // in the .cpp body's banner (the PPC float-arg GPR skip is what hid it).
        void ResetRaceCar(u32 luRaceCarIndex, Matrix44Affine lInitialTransform,
                          Vector3 lInitialVelocity, Vector3 lAngularVelocity,
                          u8 lu8VehicleListIndexUnused,
                          bool lbResetTransform, bool lbResetDeformation,
                          f32 lfRoadRageHowCloseToWrecked, bool lbResettingAfterWreck,
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

        // ⭐ ADDED 2026-08-11 (triangle-cache wiring wave) -- THE LAST HOP OF THE CHAIN THAT
        // FEEDS EVERY VEHICLE LINE TEST. Real out-of-line X360 symbol @0x8279B978 (25 insns),
        // DWARF-declared at BrnVehicleInputInterface.h:171; body in the sibling .cpp.
        //
        // ⚠️ NOTE WHAT DOES **NOT** CARRY IT: VehicleInputInterface::Append @0x823C87C0 is a
        // FOURTEEN-QUEUE merge that emits NO store for mTriangleCacheInterface (confirmed in
        // its asm -- the 14 `bl`s are all *Event_::Append). So the four world->physics bridges
        // that call Append never seed the cache; this dedicated entry point, driven once per
        // frame by WorldModule::BridgeSceneQueryResultsToPhysics @0x827A8FDC, is the only path.
        void AppendTriangleCacheInterface(const InTriangleCacheInterface* lpTriangleCacheInterface);

        // ⭐ ADDED 2026-08-11 (triangle-cache wiring wave). DWARF-declared (BrnVehicleInputInterface.h,
        // `void AddLineTestResult(OutEventLineTestNearestResult)` -- BY VALUE, and the console agrees:
        // BridgeSceneQueryResultsToPhysics copies the 64-byte event off the variable queue into a
        // stack temporary (`ld/std` x8 loop @0x827A8F74) before the call). No out-of-line symbol --
        // the X360 INLINES it to `BaseEventQueue<OutEventLineTestNearestResult>::AddEvent(this+0, &tmp)`
        // (`bl sub_827A5780` with r3 still the interface base, i.e. &mLineTestResultsQueue at +0), so
        // it is a header-only inline here, same shape as the accessors above.
        void AddLineTestResult(CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult lResult)
        {
            mLineTestResultsQueue.AddEvent(lResult);
        }

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

        // ⭐ ADDED 2026-08-11 (create-drain wave). FOUR MORE OF THE SAME SHAPE -- every one is
        // DWARF-declared (BrnVehicleInputInterface.h:192 / :219 / :195 / :198 / :228), every one is
        // inlined by the X360 as a raw `this + <byte offset>` at the maintenance-arm drain that
        // reads it, and every offset below is that raw literal reproduced by NAME:
        //   mRemoveRaceCarEventQueue                  +129328  ProcessRemoveEvents @0x826160E4
        //                                                      (`addis r3,r4,2 ; addi r3,r3,-0x6D0`)
        //   mValidateRaceCarEventQueue                +131472  ProcessValidationEvents @0x825E901C
        //                                                      (`addis r27,r4,2 ; addi r27,r27,0x190`)
        //   mSetRaceCarCollisionEventQueue            +131744  ProcessCollisionEvents @0x825E8F3C
        //                                                      (`addis r30,r28,2 ; addi r30,r30,0x2A0`)
        //   mSetRaceCarCullingGroupEventQueue         +131836  ProcessCollisionEvents @0x825E8FA8
        //                                                      (`addis r30,r28,2 ; addi r30,r30,0x2FC`)
        //   mNetworkCarsAddedRemovedForCollisionQueue +131928  RecordNetworkRaceCarsAddedForCollision
        //                                                      @0x825C7F08 (`addis r3,r31,2 ; addi r3,r3,0x358`)
        // ⭐ THE FIVE LITERALS CROSS-CHECK THE WHOLE MEMBER RUN, which is why they are quoted: run
        // the declared element sizes forward from mCreateRaceCarEventQueue's attested +128032 and
        // every one of them is hit with ZERO slack:
        //   128032 + (16 + 8*160) = 129328   mRemoveRaceCarEventQueue
        //          + (16 + 8*8)   = 129408   mResetRaceCarEventQueue
        //          + (16 + 16*128) = 131472  mValidateRaceCarEventQueue
        //          + (16 + 8*32)  = 131744   mSetRaceCarCollisionEventQueue
        //          + 92           = 131836   mSetRaceCarCullingGroupEventQueue   (12 + 10*8)
        //          + 92           = 131928   mNetworkCarsAddedRemovedForCollisionQueue
        // ⚠️ CORRECTED 2026-08-11: the reset step used to read `(16 + 16*112) = 131216`, and every
        // sum after it was mislabelled (131216 + 272 is 131488, not the attested 131472 -- the chain
        // did not close). sizeof(ResetVehicleEvent) on the console is **128, not 112**: u32 index (4)
        // + pad to the struct's own 16-byte alignment (16) + Matrix44Affine (64 -> 80) + two Vector3
        // (32 -> 112) + three bools + f32 + the DeformationResetType enum (-> 124), and the
        // `alignas(16)` on the struct tail-pads that 124 up to 128. With 128 the reset queue is
        // (16 + 16*128) = 2064 bytes and lands mValidateRaceCarEventQueue exactly on its attested
        // +131472. (The 12-byte queue headers are the 4-aligned element types -- BaseEventQueue<T>
        // is {T*, s32, s32}, so the inline array starts at +12 when T needs no more than 4-byte
        // alignment and at +16 when it needs 16.)
        const RemoveRaceCarEventQueue*   GetRemoveRaceCarEvents() const   { return &mRemoveRaceCarEventQueue; }

        // ⭐ [teleport] ADDED 2026-08-21 (gateui r9, the reset-drain wave). Same shape and the
        // same derivation as the five above: DWARF-declared (BrnVehicleInputInterface.h, the
        // Get*Events run), no out-of-line X360 symbol, and inlined by the console at its ONE
        // consumer -- VehicleManager::ProcessResetEvents @0x82617820 opens with
        //     0x82617B08  result = (a2 + 129408)          -- the pseudocode's own literal
        //     0x82617E58  lwz r11, 8(result)              -- BaseEventQueue::miLength
        // and 129408 is EXACTLY this member's seat in the run the banner above computes
        // (128032 + (16 + 8*160) = 129328, + (16 + 8*8) = 129408). The Append body in the
        // sibling .cpp already cites the same +0x1F980. Reached by NAME here.
        const ResetRaceCarEventQueue*    GetResetRaceCarEvents() const    { return &mResetRaceCarEventQueue; }
        const ValidateRaceCarEventQueue* GetValidateRaceCarEvents() const { return &mValidateRaceCarEventQueue; }
        const SetRaceCarCollisionEventQueue* GetSetRaceCarCollisionEvents() const
        {
            return &mSetRaceCarCollisionEventQueue;
        }
        const SetRaceCarCullingGroupEventQueue* GetSetRaceCarCullingGroupEvents() const
        {
            return &mSetRaceCarCullingGroupEventQueue;
        }
        const NetworkCarsAddRemoveForCollisionQueue* GetNetworkCarsAddRemoveForCollisionQueue() const
        {
            return &mNetworkCarsAddedRemovedForCollisionQueue;
        }
        // ⭐ THE REMOVE QUEUE'S SEAT WAS DERIVED TWICE, 2026-08-11, by two independent waves reading
        // the same three instructions -- and the cross-check caught an arithmetic slip. The second
        // wave quoted the same `addis r3,r4,2 ; addi r3,r3,-0x6D0 ; lwz r11,8(r3)` prologue but
        // evaluated it as +130352 (and as mCreateRaceCarEventQueue's +128032 plus a 2320-byte
        // queue). Both are wrong by 1024: 0x20000 == 131072, 131072 - 0x6D0 (1744) == **129328**,
        // which is 128032 + (16 + 8*160) == +1296, the create queue's real size. The seat above is
        // the correct one, and it is the only one either body reaches -- BY NAME, through
        // GetRemoveRaceCarEvents(), which is the DWARF's own spelling (:192). No second accessor
        // under a different name is added: one member, one attested name.

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

        // ⭐ ADDED 2026-08-22 (wave T3 r1, C2 -- the physics-side create/remove drain). The three
        // TRAFFIC queue readers, DWARF-attested (BrnVehicleInputInterface.h:207 / :225 / :201) and
        // spelled with the DWARF's own names. Same shape as the race-car readers above: header
        // inlines with no out-of-line X360 symbol, folded at their call sites as a bare
        // `addis/addi` off the interface --
        //     0x8264970C  addis r4, r29, 2 ; addi r4, r4, 0x770   == +132976 mCreateTrafficEventQueue
        //     0x82649734  addis r4, r29, 2 ; addi r4, r4, 0x1590  == +136592 mCreateArticulatedTrafficEventQueue
        //     0x82649AA0  addis r4, r26, 2 ; addi r4, r4, 0x2118  == +139544 mRemoveCrashedTrafficEventQueue
        // (PhysicalTrafficManager::ProcessCreateEvents / ::ProcessTrafficMaintenanceEvents).
        // Each is the same member the console addresses, reached by name instead of by that offset.
        const CreateTrafficEventQueue* GetCreateTrafficBodyEvents() const
        {
            return &mCreateTrafficEventQueue;
        }
        const CreateArticulatedTrafficEventQueue* GetCreateArticulatedTrafficQueue() const
        {
            return &mCreateArticulatedTrafficEventQueue;
        }
        const RemoveTrafficEventQueue* GetRemoveTrafficEvents() const
        {
            return &mRemoveCrashedTrafficEventQueue;
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
