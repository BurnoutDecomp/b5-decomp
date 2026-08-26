// ============================================================================
// b5-decomp/src/GameSource/Physics/VehicleManager/BrnVehicleManager_UpdateVehiclePhysics.cpp
//
// BIG FIVE #3 -- BrnPhysics::Vehicle::VehicleManager::UpdateVehiclePhysics @0x82644FA8
// (1,038 X360 instructions), the per-frame FORCE PRODUCER: the manager-level conductor
// that dispatches every live car's driver + physics update (the seam into the 54 bodied
// force leaves), plus its in-TU per-frame siblings:
//
//   UpdateVehiclePhysics                @0x82644FA8  (1038)  DWARF h:896
//   IsRaceCarCrashing                   @0x825B5690  (30)    DWARF h:947
//   ForceRaceCarCrash (5-arg overload)  @0x82635B78  (34)    DWARF h:1239
//   ProcessAboveGroundLineTestsResults  @0x826183F8  (105)   DWARF h:866
//   ProcessAftertouchEvents             @0x82633DE8  (69)    DWARF h:1113
//
// Home TU BrnVehicleManager.cpp is still unmounted -- this is a slice TU in the
// established BrnVehicleManager_PerFrameLeaves.cpp pattern (fold back at the home mount).
//
// Every body is reconstructed from its X360 asm read line by line (the Hex-Rays for the
// conductor renders the BitArray iteration as ~200 lines of inlined streamed-assert
// machinery -- that all lives inside CgsBitArray.h's Get{First,Next}NonZeroBit and is NOT
// re-expanded here). Members are accessed BY NAME; every console offset the asm touches
// is cited at its use and lands on a member the committed headers already pin.
//
// sub_82635B78 IDENTITY (previously unnamed in the export set): it is the FIVE-argument
// ForceRaceCarCrash overload. Proof, three-legged: (1) DWARF declares the overload PAIR at
// BrnVehicleManager.h:1239 (5-arg) / :1242 (6-arg, + EntityId) and MSVC lays adjacent
// overloads in reversed declaration order -- 0x82635B00 (named ForceRaceCarCrash in the
// exports) is the 6-arg form, 0x82635B78 the 5-arg; (2) caller set: both in-body call
// sites here pass exactly this+5 with NO EntityId while CrashFatalRaceCars (which holds an
// EntityId) calls 0x82635B00; (3) raw image bytes (x360rd, self-test 10/10): 0x82635B78
// clears the driver's two invulnerability bytes, gates on !mbCrashing (RCP+0x710), and
// `bl 0x82634C90` (SetRaceCarCrashing) with r10 = -1 (the "no takedown type" sentinel)
// and both contact vectors zero -- while 0x82635B00 passes its EntityId parameter through.
//
// TRAP-STUB TAILS THIS TU ARMS (all dead until PhysicsModule::Update lands -- see
// BrnVehicleManagerLinkStubs.cpp): SetRaceCarCrashing, UpdateVehicleImpacts,
// UpdateAggressiveDriving, UpdateCrashes, EndVehicleTractionLineTests, CrashFatalRaceCars,
// ReadSurfaceProperties(u64), VehicleDriver::UpdateVehicle, PhysicalTrafficManager::
// UpdateTrafficPhysics, PhysicalTrafficManager::PassNearbyCrashingTrafficIdsToRaceCarModule,
// DebugComponent::Update.
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerPerfMonHandles.h" // the 13 stage monitors (defined in _Construct.cpp)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"  // VehicleInputInterface (queues)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // VehicleOutputInterface / VehicleManagerOutputInterface / VehicleOutputRequestInterface
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"         // CgsDev::PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTestNearest.h" // InEventLineTestNearest (GenerateAboveGroundLineTests)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                  // CgsModule::VariableEventQueue<1536,16> (the GameEventQueue)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                          // CgsSystem::Time (mCurrentTime = lrCurrentTime)
#include "SharedClasses/BrnSharedConstants.h"                                     // BrnUpdateSet
#include "rw/math/vpu/vector3_operation.h"                                        // vpu::{Magnitude, Normalize, Dot, Abs, Add, Mult}
#include "GameSource/Physics/VehicleManager/VehiclePhysics/B5PhysicsHandlingDebugComponent.h" // BrnPhysics::Vehicle::DebugComponent (per-car tick)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h" // Attrib::StringToKey
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                        // gpDebugPrint ([crash-probe] witness)
#include <cstdlib>                                                                // getenv / atoi ([crash-probe] trigger)

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    // [T5-ram] DIAG state, DEFINED in BrnPhysicalTrafficManager_UpdateTrafficPhysics.cpp.
    // NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    extern s32 gT5RamFramesLeft;
    extern f32 gT5PlayerPos[3];
    extern f32 gT5PlayerVel[3];

    // ------------------------------------------------------------------------------------
    // File-scope statics of the console BrnVehicleManager.cpp translation unit that this
    // slice's bodies read.
    // ------------------------------------------------------------------------------------

    // byte_82FBA344 -- DWARF BrnVehicleManager.cpp:2585 `bool _lbOverrideForceFrozen`, the
    // debug "freeze the player car regardless of the update set" override. FLAG: its
    // console WRITER (a debug window) is not reconstructed, so on PC this stays false and
    // the `||` arm below is statically dead -- carried as shipped, not pruned.
    static bool gs_bOverrideForceFrozen = false;

    // qword_82FB7F10 -- the 64-bit AttribSys surface-list key UpdateVehiclePhysics passes
    // to ReadSurfaceProperties (`ld r4, qword_82FB7F10` @0x82645144). Breaker static
    // initializer @0x82C5B5F8 calls Attrib::StringToKey("340654") and stores the u64 result.
    // DWARF spells the parameter type Attribute::Key; the committed Attribute::Key typedef
    // is u32 while the console load is 8 bytes -- width conflict FLAGGED at the
    // declaration (BrnVehicleManager.h), not compounded here.
    static const u64 gs_uSurfaceListKey = Attrib::StringToKey("340654");

    // The stationary-pose wheel-angle scale (rodata flt_8208FA8C == 1.3f, read off the
    // image). DWARF BrnVehicleManager.cpp:223 names the constant
    // KF_WHEEL_ANGLE_IN_CAR_SELECT_SCALE; the meStationaryPlayerWheelAngle == 0 arm uses
    // the NEGATED value from a local literal (asm 0x82645F00 lfs flt_8209D0D4 == -1.3f).
    static const f32 KF_WHEEL_ANGLE_IN_CAR_SELECT_SCALE = 1.3f;

    // rodata 0x8208F5F4 == 0.01745329238f (pi/180). The console reaches it as
    // `lfs -0x3E0(&flt_8208F9D4)`; the DWARF hint spells the operation rw::math::vpu::DegToRad.
    static const f32 KF_DEG_TO_RAD = 0.01745329238474369f;

    // The handbrake-turn crash gate thresholds (both image-read):
    //   flt_8208F9D4 = 20.0f  -- minimum speed for the "sliding, not driving" crash test
    //   flt_82001DA0 = 0.5f   -- cos-angle floor: dot(forward, velocity-dir) below this crashes
    static const f32 KF_HANDBRAKE_CRASH_MIN_SPEED   = 20.0f;
    static const f32 KF_HANDBRAKE_CRASH_MAX_COS_DOT = 0.5f;

    // ------------------------------------------------------------------------------------
    // IsRaceCarCrashing  @0x825B5690  (30 insns)
    // Bounds-assert the index (console lines :8272/:8273) and return the car's crash
    // master flag (lbz RCP+0xE50-0x740 == SimpleVehiclePhysics::mbCrashing @+0x710).
    // ------------------------------------------------------------------------------------
    bool VehicleManager::IsRaceCarCrashing(s32 liRaceCarIndex)
    {
        CGS_ASSERT(liRaceCarIndex >= 0, "liRaceCarIndex >= 0");                    // :8272
        CGS_ASSERT(liRaceCarIndex < 8, "liRaceCarIndex < ku8MaxNumRaceCars");      // :8273
        return maRaceCarVehicles[liRaceCarIndex].IsCrashing();                     // lbz 0xE50(rec)
    }

    // ------------------------------------------------------------------------------------
    // ForceRaceCarCrash (5-arg overload)  @0x82635B78  (34 insns) -- see the identity
    // banner at the top of this file.
    //
    // Clear the target driver's two invulnerability latches, then (unless the car is
    // already crashing) commit a crash through the universal sink with the car's own
    // entity id as both victim AND aggressor, zero contact geometry, and no takedown type:
    //   0x82635BA4/A8  stb 0, 0x7C/0x7D(this+224*idx)   -> driver+0x3C/+0x3D (invuln flags)
    //   0x82635BAC     ld  (this + 8*(idx+5468))        -> maRaceCarHandlingBodyIDs[idx]
    //   0x82635BB4/B8  srdi 32 / clrldi                 -> the EntityId is the HIGH word
    //                  (same extraction the PTM contact fix-up documents for RigidBodyId)
    //   0x82635BBC/C0  lbz 0xE50(this+5216*idx) gate    -> !mbCrashing
    //   0x82635BC8/DC  vspltisw v2,0 ; vmr v1,v2        -> both contact vectors zero
    //   0x82635BD8     li r10, -1                       -> ETakedownType "none"
    //   0x82635BE8     bl SetRaceCarCrashing @0x82634C90
    // ------------------------------------------------------------------------------------
    void VehicleManager::ForceRaceCarCrash(
        BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
        VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
        VehicleOutputInterface* lpVehicleOutputInterface,
        BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
        EActiveRaceCarIndex leRaceCarIndex)
    {
        maRaceCarDrivers[leRaceCarIndex].SetInvulnerableToVehicles(false);   // stb 0, +0x7C
        maRaceCarDrivers[leRaceCarIndex].SetInvulnerableToWorld(false);      // stb 0, +0x7D

        if (!maRaceCarVehicles[leRaceCarIndex].IsCrashing())                 // lbz 0xE50 gate
        {
            // RigidBodyId keeps the EntityId in its HIGH 32 bits (ld ; srdi 32).
            const u32 luEntityId =
                static_cast<u32>(maRaceCarHandlingBodyIDs[leRaceCarIndex] >> 32);

            // CAST RETIRED 2026-08-11 (consolidation wave). SetRaceCarCrashing's committed
            // declaration used to type its output-interface parameter
            // BrnGameState::GameStateModuleIO::VehicleOutputInterface* -- a fork type that does
            // not exist in the DWARF at all -- while the DWARF (BrnVehicleManager.h:1218) and
            // this function's own signature use the BrnPhysics::Vehicle::VehicleOutputInterface
            // this TU carries (mangle `...PNS0_22VehicleOutputInterfaceE...`, NS0_ ==
            // BrnPhysics::Vehicle). The declaration is re-typed, so the pointer passes straight
            // through.
            // The manager decl's EntityId is BrnCommonTypes' packed-id POD (`{ u32 muValue; }`).
            EntityId lVictimId;
            lVictimId.muValue = luEntityId;
            SetRaceCarCrashing(
                lVictimId,
                lVictimId,
                Vector3{ 0.0f, 0.0f, 0.0f, 0.0f },
                Vector3{ 0.0f, 0.0f, 0.0f, 0.0f },
                lpRequestOutputInterface,
                lpVehicleManagerOutputInterface,
                lpVehicleOutputInterface,
                lpDeformationInterface,
                static_cast<BrnGameState::ETakedownType>(-1));               // li r10, -1
        }
    }

    // ------------------------------------------------------------------------------------
    // GenerateAboveGroundLineTests  @0x82633990  (109 insns)   -- THE PRODUCER
    //
    // For every LIVE race car, post one downward 10 m "nearest" line test into the physics
    // module's outgoing fine-query queue. The result comes back next frame through
    // WorldBridgeSceneToPhysics -> VehicleInputInterface::AddLineTestResult ->
    // ProcessAboveGroundLineTestsResults (immediately below), which stamps the car's
    // AboveGroundTestResult -- the thing RCEM::UpdateRaceCarCollisionTagging needs before a car
    // can be given an AI section index.
    //
    // Console body (r3 == this, r4 == lpRequestInterface):
    //   0x826339A0  the bit-array base: `addis r14,r3,1 ; addi r14,r14,-0x5340` == this+44224
    //               == &mUsedRaceCars, then the standard CgsBitArray GetFirstNonZeroBit /
    //               GetNextNonZeroBit walk (its own CgsBitArray.h:203 index tripwire inline).
    //   0x82633A24  the queue seat: `addi r15, r4, 0x28C0` == &mRequestFineLineQueue
    //   0x82633A74  `mulli r10, r31, 0x1460 ; add r10,r10,r3 ; lvx128 v0, r10, 0x780`
    //               == maRaceCarVehicles[i] (base +1856, stride 5216) + 0x40 == GetPosition()
    //   0x82633AD8  `vsubfp v0,v0,v13 ; vrlimi128 v12,v0,4,0` -- v13 is a splat of
    //               flt_82004A20 (READ FROM THE IMAGE: 10.0f) and vrlimi mask 4 selects WORD 1,
    //               so ONLY Y is lowered: the end point is the start point 10 m straight down.
    //   0x82633ABC  `clrlslwi r10,r9,24,8 ; oris r10,r10,2` -> mQueryId == 0x20000 | (i << 8).
    //               ⭐ THAT ENCODING IS THE CONTRACT WITH THE RESULT HALF: the consumer below
    //               reads request type as (id >> 16) & 0xFF (must be 2) and the car index as
    //               (id >> 8) & 0xFF. Producer and consumer are 130 KB apart in the image and
    //               nothing but this pair of shifts ties them together.
    //   0x82633A80  mx32EntityTypeFlags = 2 ; 0x82633AC0 mExcludeEntityId = dword_82F2A3A4
    //               (READ FROM THE IMAGE: 0xFFFFFFFF == EntityId::SetInvalid) ;
    //               0x82633A88 meExclusionMode = 0 ; 0x82633A84 mxVolumeTypeFlags = 2
    //   0x82633AEC  AddEvent<InEventLineTestNearest>(&event, 6)
    // ------------------------------------------------------------------------------------
    void VehicleManager::GenerateAboveGroundLineTests(
        VehicleOutputRequestInterface* lpRequestInterface)
    {
        CGS_ASSERT(lpRequestInterface != 0, "lpRequestInterface != NULL");
        if (lpRequestInterface == 0)
        {
            return;
        }

        // flt_82004A20, read out of the decrypted ARTIST image (0x41200000).
        static const f32 KF_ABOVE_GROUND_RAY_LENGTH = 10.0f;
        // The (id >> 16) & 0xFF value ProcessAboveGroundLineTestsResults tests for.
        static const u32 KU_REQUEST_TYPE_RACE_CAR_ABOVE_GROUND = 2u;
        // The console's `li r5, 6` -- the scene-query event TYPE id the variable queue tags the
        // record with. (3 is the traffic traction line test the physical-traffic manager posts;
        // the SceneManager's query dispatcher switches on this.)
        static const s32 KI_SCENE_QUERY_EVENT_LINE_TEST_NEAREST = 6;

        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar >= 0;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            // [FLAG PC hardening] value-initialised. The console builds this record on the
            // stack and leaves the 15 pad bytes after mxVolumeTypeFlags as whatever was there;
            // AddEvent block-copies all 64. Nothing reads the pad, so zeroing it is invisible
            // -- and it keeps an uninitialised read off a shared queue.
            CgsSceneManager::SceneManagerIO::InEventLineTestNearest lEvent = {};

            const Vector3 lStart = maRaceCarVehicles[liCar].GetPosition();
            Vector3 lEnd = lStart;
            lEnd.y -= KF_ABOVE_GROUND_RAY_LENGTH;

            lEvent.mLineStart          = lStart;
            lEvent.mLineEnd            = lEnd;
            lEvent.mQueryId.mId        = (KU_REQUEST_TYPE_RACE_CAR_ABOVE_GROUND << 16) |
                                         (static_cast<u32>(liCar) << 8);
            lEvent.mx32EntityTypeFlags = 2u;
            lEvent.mExcludeEntityId.SetInvalid();
            lEvent.meExclusionMode     =
                CgsSceneManager::SceneManagerIO::E_NEAREST_EXCLUDE_ENTITY_ONLY;
            lEvent.mxVolumeTypeFlags   = 2u;

            lpRequestInterface->GetRequestFineLineQueue()
                ->AddEvent<CgsSceneManager::SceneManagerIO::InEventLineTestNearest>(
                    &lEvent, KI_SCENE_QUERY_EVENT_LINE_TEST_NEAREST);
        }
    }

    // ------------------------------------------------------------------------------------
    // ProcessAboveGroundLineTestsResults  @0x826183F8  (105 insns)
    // Drain the scene manager's line-test result queue: every VALID result of request type
    // 2 (a race car's above-ground down-ray) stamps that car's AboveGroundTestResult via
    // the already-bodied SimpleVehiclePhysics::SetAboveGroundTestResult; type 3 (traffic)
    // and unknown types fire the console's own streamed asserts (lowered to CGS_ASSERT
    // with the static message per the standing rule).
    //
    // Event decode (the committed OutEventLineTestNearestResult layout, producer-pinned in
    // CgsSceneManagerModuleIO.h; the console copies each 64-byte record to the stack ld x8):
    //   +0x00 mPosition (v1)   +0x10 mNormal (v2)   +0x28 mQueryId {.., type, index, ..}
    //   +0x34 mu16MaterialTag  +0x36 mu16GroupTag   +0x38 mbIntersection
    //   (extrwi 8,8 -> request type; extrwi 8,16 -> car index; asm 0x82618480..0x82618580)
    // ------------------------------------------------------------------------------------
    void VehicleManager::ProcessAboveGroundLineTestsResults(
        const VehicleInputInterface::InLineTestResultQueue* lpLineTestResults)
    {
        for (s32 liEvent = 0; liEvent < lpLineTestResults->GetLength(); ++liEvent)
        {
            const CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult& lrResult =
                lpLineTestResults->GetEvent(liEvent);

            if (!lrResult.mbIntersection)
                continue;

            const u32 luRequestType  = (lrResult.mQueryId.mId >> 16) & 0xFFu;   // extrwi 8,8
            const u32 luRaceCarIndex = (lrResult.mQueryId.mId >> 8) & 0xFFu;    // extrwi 8,16

            if (luRequestType == 2u)
            {
                maRaceCarVehicles[luRaceCarIndex].SetAboveGroundTestResult(
                    lrResult.mPosition, lrResult.mNormal,
                    lrResult.mu16MaterialTag, lrResult.mu16GroupTag);
            }
            else if (luRequestType == 3u)
            {
                CGS_ASSERT(false,
                    "A physical traffic vehicle tried submitted an 'above ground' line test to the SceneManager");   // :2752
            }
            else
            {
                CGS_ASSERT(false, "Received result for an unknown line test request");  // :2759
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // ProcessAftertouchEvents  @0x82633DE8  (69 insns)
    // Only while the car is CRASHING: publish this frame's aftertouch stick values (event
    // 76, 12 bytes {index, y, x}), and -- while the player car is actually in showtime --
    // the recent-bounce report (event 52, 32 bytes) and the one-shot sixaxis-tilt latch
    // (event 53, 1 byte; the latch byte_82FB848A == msPlayerParams.mbSixaxisTiltApplied
    // is CONSUMED here: read then cleared, asm 0x82633ED0/0x82633ED4).
    //
    // THE GetAftertouchValues FORK RESOLVES AT THIS CALL SITE. The out-of-line leaf
    // @0x825B2E88 is called here (bl @0x82633E44) with FOUR arguments -- three float
    // pointers AND the bool r7 = (meShowtimeBehaviour == 2) -- and no return read: it is
    // the reference form `void GetAftertouchValues(f32&, f32&, f32&, bool) const` bodied
    // at BrnPlayerDriverControls.cpp:39. The 3-pointer `bool GetAftertouchValues(f32*,
    // f32*, f32*)` declaration was the fork no TU could define; it is deleted this wave
    // (BrnVehicleDriverControls.h) and UpdateAftertouch's call is re-pointed.
    // ------------------------------------------------------------------------------------
    void VehicleManager::ProcessAftertouchEvents(
        s32 liRaceCarIndex, CgsModule::VariableEventQueue<1536, 16>* lpOutputQueue)
    {
        RaceCarPhysics& lrCar = maRaceCarVehicles[liRaceCarIndex];

        if (!lrCar.IsCrashing())                                   // lbz 0xE50 gate
            return;

        // Event 76 (0x4C): the per-frame aftertouch values. 12-byte payload, in the
        // console's exact stack order {s32 index, f32 out2, f32 out1} (stw idx @sp+0x58,
        // outs written by the callee at sp+0x5C / sp+0x60; AddEvent copies from sp+0x58).
        struct AftertouchEventPayload
        {
            s32 miRaceCarIndex;
            f32 mfAftertouchY;
            f32 mfAftertouchX;
        };
        AftertouchEventPayload lPayload;
        lPayload.miRaceCarIndex = liRaceCarIndex;

        f32 lfOutZ = 0.0f;   // sp+0x54 -- written by the callee, NOT part of the event
        maRaceCarDrivers[liRaceCarIndex].mControls.GetAftertouchValues(
            lPayload.mfAftertouchX, lPayload.mfAftertouchY, lfOutZ,
            meShowtimeBehaviour == 2u);                            // r7: cntlzw/extrwi idiom

        lpOutputQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lPayload), 76, 12);

        // Showtime-only tail: virtual slot +0x14 == IsPlayerVehicleActuallyInShowtime.
        if (lrCar.IsPlayerVehicleActuallyInShowtime())
        {
            // Event 52 (0x34): the recent-bounce report GetRecentBounce fills, 32 bytes in
            // the console's exact stack order (sp+0x40..sp+0x60: chain count @+0, five
            // flag bytes @+4.., other-entity id @+8, tail padding as laid out).
            struct alignas(16) RecentBounceEventPayload
            {
                s32  miChainCount;        // sp+0x40  (GetRecentBounce a2)
                bool mbOverMinStress;     // sp+0x44  (a3)
                bool mbCarBounce;         // sp+0x45  (a4)
                bool mbGoodImpact;        // sp+0x46  (a5)
                bool mbShouldBounceBoost; // sp+0x47  (a6)
                s32  miOtherEntityId;     // sp+0x48  (a7)
                u8   mau8Tail[20];        // sp+0x4C..0x60 -- queued verbatim, never read back typed
            };
            RecentBounceEventPayload lBounce = {};
            // r10 (the 7th out) points at payload+0x10 -- the bounce DIRECTION vector's
            // 16-aligned slot inside the queued record (mau8Tail[4..20)).
            if (lrCar.GetRecentBounce(&lBounce.miChainCount, &lBounce.mbOverMinStress,
                                      &lBounce.mbCarBounce, &lBounce.mbGoodImpact,
                                      &lBounce.mbShouldBounceBoost, &lBounce.miOtherEntityId,
                                      reinterpret_cast<Vector3*>(&lBounce.mau8Tail[4])))
            {
                lpOutputQueue->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lBounce), 52, 32);
            }

            // Event 53 (0x35): one-shot sixaxis-tilt notification. Consume the latch.
            const bool lbTiltApplied = msPlayerParams.mbSixaxisTiltApplied;   // byte_82FB848A
            msPlayerParams.mbSixaxisTiltApplied = false;
            if (lbTiltApplied)
            {
                const u8 lu8Payload = 0;   // sp+0x50 -- one uninitialised-on-console byte; zeroed here
                lpOutputQueue->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lu8Payload), 53, 1);
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // UpdateVehiclePhysics  @0x82644FA8  (1,038 insns) -- THE FORCE PRODUCER.
    // Stage order and every monitor bracket are the console's; the 13 handles line up 1:1
    // with the call sequence (gs_iUpdateVehicleImpactsPM .. gs_iUpdatePassBysPM).
    // ------------------------------------------------------------------------------------
    void VehicleManager::UpdateVehiclePhysics(
        CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* lpOutputBufferStack,
        BrnUpdateSet lUpdateSet,
        CgsSystem::Time& lrCurrentTime,
        f32 lfSimTimerTimeStep,
        f32 lfGameTimerTimeStep,
        const VehicleInputInterface* lpInputInterface,
        VehicleOutputInterface* lpVehicleOutputInterface,
        BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
        VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
        BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
        bool lbIsOnlineGameMode,
        CgsSceneManager::EntityId lWorldEntityId)
    {
        CGS_ASSERT(lpInputBufferStack != NULL, "lpInputBufferStack != NULL");                      // :2443
        CGS_ASSERT(lpOutputBufferStack != NULL, "lpOutputBufferStack != NULL");                    // :2444
        CGS_ASSERT(lpInputInterface != NULL, "lpInputInterface != NULL");                          // :2445
        CGS_ASSERT(lpVehicleOutputInterface != NULL, "lpVehicleOutputInterface != NULL");          // :2446
        CGS_ASSERT(lpRequestOutputInterface != NULL, "lpRequestOutputInterface != NULL");          // :2447
        CGS_ASSERT(lpVehicleManagerOutputInterface != NULL, "lpVehicleManagerOutputInterface != NULL"); // :2448
        CGS_ASSERT(lpDeformationInterface != NULL, "lpDeformationInterface != NULL");              // :2449

        // ---- STAGE: impacts (dword_82F2A14C) -------------------------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iUpdateVehicleImpactsPM);

        // Player pressed RESET -> re-read the surface-property table (asm 0x82645110..48;
        // gate = maRaceCarDrivers[player].mControls.mbReset, the +0x39 byte).
        if (mePlayerActiveRaceCarIndex != -1 &&
            maRaceCarDrivers[mePlayerActiveRaceCarIndex].mControls.mbReset)
        {
            ReadSurfaceProperties(gs_uSurfaceListKey);             // ld qword_82FB7F10
        }

        mCurrentTime = lrCurrentTime;   // stw +0 / stfs +4 at this+172364 (Time == {s32,f32})

        // Online-mode retune (asm 0x82645174..0x82645220). All five stores + both branch
        // constant sets verified against the image (0.4/0.6 online, 0.2/0.4 offline,
        // 2.0/0.4/16.0 common) -- the DWARF names the online pair
        // KF_ONLINE_MIN/MAX_SHUNT_MAGNITUDE (BrnVehicleManager.cpp:201/202).
        if (mbIsOnlineGameMode != lbIsOnlineGameMode)
        {
            mbIsOnlineGameMode = lbIsOnlineGameMode;
            if (lbIsOnlineGameMode)
            {
                mfMinShuntMagnitude = 0.4f;    // flt_8200473C
                mfMaxShuntMagnitude = 0.6f;    // flt_82004D00
            }
            else
            {
                mfMinShuntMagnitude = 0.2f;    // flt_82004744
                mfMaxShuntMagnitude = 0.4f;    // flt_8200473C
            }
            mfSlamEffectMaxMagnitude = 2.0f;   // flt_82001D9C
            mfSlamEffectMinMagnitude = 0.4f;   // flt_8200473C
            mfMaxSlamClosingXSpeed   = 16.0f;  // flt_82004000
        }

        // =======================================================================================
        // [crash-probe] NOT AN X360 MECHANISM -- a TRIGGER, exactly like the BRN_CAR_TELEPORT and
        // [showtime-probe] precedents. Inert unless BRN_CRASH_PLAYER is set (one getenv on the
        // first update, one counter test per frame after).
        //
        // WHY IT EXISTS. mbCrashPlayerNextUpdate (+172309) is a DEBUG-MENU bool: an image-wide
        // census over all 30,084 ARTIST exports finds exactly two touches -- Construct writes
        // false, and the block below reads-and-clears it. NOTHING in the image sets it, because
        // the thing that set it was the dev menu, which this build has no route to. So the whole
        // player-crash commit chain below (ForceRaceCarCrash 5-arg -> SetRaceCarCrashing ->
        // AddRaceCarCrashEvent + RaceCarPhysics::SetCrashing) is real, mounted, bodied -- and
        // COLD. Setting the flag invents no path: it is the one input the console's own debug
        // build supplied, and every line after it is the console's.
        //
        // ⚠ DELIBERATELY NOT RaceCarPhysics::SetCrashing(true). The existing [showtime-probe]
        // calls that directly, which is PHYSICS-LOCAL: no crash record is allocated, no
        // RaceCarCrashEvent is posted, and ActiveRaceCar / the GUI / CrashPlayManager never learn.
        // That probe is not evidence for this chain. This one drives the real commit.
        //
        // BRN_CRASH_PLAYER = the number of UpdateVehiclePhysics calls to wait before firing ONCE
        // (default 900 ~= 15 s at 60 Hz, i.e. after the drive is under way). Fires once per boot.
        {
            static const char* const kspCrashProbe = getenv("BRN_CRASH_PLAYER");
            if (kspCrashProbe != 0)
            {
                static u32 sluProbeFrames = 0;
                static bool sbProbeFired  = false;
                const u32 kluProbeAt = (atoi(kspCrashProbe) > 0)
                                     ? static_cast<u32>(atoi(kspCrashProbe)) : 900u;
                ++sluProbeFrames;
                // ⭐ TWO ARMS SINCE THE 2026-08-25 BRING-UP FLAG (BrnVehicleManager.cpp::
                // SetRaceCarCrashing). They differ ONLY in how the sink is reached, and that
                // difference exists for one reason: the console's deferred-request block two
                // blocks below ends in CGS_ASSERT(IsRaceCarCrashing(player)). With crash entry
                // disabled that assert would fire -- CORRECTLY, the sink did return early -- and a
                // dev assert PAUSES the game waiting for END, hanging a run whose author simply
                // forgot BRN_ENABLE_CRASH_ENTRY.
                //   * enabled  -> set mbCrashPlayerNextUpdate, i.e. the console's own path, assert
                //                 and all. Nothing about it changes.
                //   * disabled -> call ForceRaceCarCrash straight from this (already non-X360)
                //                 probe block, so the sink is GENUINELY REACHED and prints its own
                //                 "[bringup] crash entry suppressed" line, and skip the console
                //                 request that carries the assert.
                // ⭐ THAT IS THE POINT: the same stimulus, deterministically, on both sides of the
                // flag -- so the flag can be shown to be load-bearing at the sink instead of being
                // taken on trust from a stochastic traffic collision that may not happen at all.
                static const bool sbCrashEntryEnabled = (getenv("BRN_ENABLE_CRASH_ENTRY") != 0);
                if (!sbProbeFired && sluProbeFrames >= kluProbeAt)
                {
                    sbProbeFired = true;
                    if (sbCrashEntryEnabled)
                    {
                        mbCrashPlayerNextUpdate = true;
                        if (CgsDev::Log::gpDebugPrint != 0)
                        {
                            *CgsDev::Log::gpDebugPrint
                                << "[crash-probe] frame " << sluProbeFrames
                                << ": setting mbCrashPlayerNextUpdate (player slot "
                                << static_cast<s32>(mePlayerActiveRaceCarIndex) << ")\n";
                        }
                    }
                    else
                    {
                        if (CgsDev::Log::gpDebugPrint != 0)
                        {
                            *CgsDev::Log::gpDebugPrint
                                << "[crash-probe] frame " << sluProbeFrames
                                << ": crash entry is DISABLED (BRN_ENABLE_CRASH_ENTRY unset) --"
                                << " driving the sink anyway to witness the suppression, and"
                                << " skipping the console's post-condition assert (player slot "
                                << static_cast<s32>(mePlayerActiveRaceCarIndex) << ")\n";
                        }
                        ForceRaceCarCrash(lpRequestOutputInterface, lpVehicleManagerOutputInterface,
                                          lpVehicleOutputInterface, lpDeformationInterface,
                                          mePlayerActiveRaceCarIndex);
                        if (CgsDev::Log::gpDebugPrint != 0)
                        {
                            *CgsDev::Log::gpDebugPrint
                                << "[crash-probe] ForceRaceCarCrash returned into a DISABLED sink;"
                                << " IsRaceCarCrashing(player)="
                                << (IsRaceCarCrashing(mePlayerActiveRaceCarIndex) ? 1 : 0)
                                << " (expected 0)\n";
                        }
                    }
                }
            }
        }

        // Deferred "crash the player next update" request (asm 0x82645224..0x82645298).
        if (mbCrashPlayerNextUpdate)
        {
            mbStopPlayerCrashing = false;                          // stbx 0 @+172306
            ForceRaceCarCrash(lpRequestOutputInterface, lpVehicleManagerOutputInterface,
                              lpVehicleOutputInterface, lpDeformationInterface,
                              mePlayerActiveRaceCarIndex);
            CGS_ASSERT(IsRaceCarCrashing(mePlayerActiveRaceCarIndex),
                       "Couldn't force player car to crash - this could have nasty consequences.");  // :2499
            mbCrashPlayerNextUpdate = false;

            // [crash-probe] witness. NOT X360. Prints the console's own post-condition so the
            // chain is proven to have RUN, not merely to have been called.
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[crash-probe] ForceRaceCarCrash returned; IsRaceCarCrashing(player)="
                    << (IsRaceCarCrashing(mePlayerActiveRaceCarIndex) ? 1 : 0) << "\n";
            }
        }

        UpdateVehicleImpacts(lpInputInterface->GetImpactEventQueue(),   // input+141376
                             lpVehicleOutputInterface, lpRequestOutputInterface,
                             lpVehicleManagerOutputInterface, lpDeformationInterface);

        CgsDev::PerfMonCpu::StopMonitor(gs_iUpdateVehicleImpactsPM);

        // ---- STAGE: above-ground results (dword_82F2A150) ------------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iProcessAboveGroundLTsPM);

        // Reset every live car's wheel/above-ground line-test latches (the loop the
        // Hex-Rays renders as the giant inlined-BitArray blob, asm 0x826452D0..0x82645628;
        // the per-car stores are SimpleVehiclePhysics::ResetAboveGroundTestResult, bodied
        // in its own TU off this exact inline).
        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar != CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            maRaceCarVehicles[liCar].ResetAboveGroundTestResult();
        }

        mPhysicalTrafficManager.ResetAboveGroundTestResults();     // bl @0x82645634
        ProcessAboveGroundLineTestsResults(lpInputInterface->GetLineTestResults());  // input+0

        CgsDev::PerfMonCpu::StopMonitor(gs_iProcessAboveGroundLTsPM);

        // ---- STAGE: traction line tests (dword_82F2A154) -------------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iTractionLTsPM);
        // RE-POINTED AGAIN 2026-08-11 (lifetime wave): TWO arguments, restoring what the
        // note removed. That note reasoned from the callee ("the 68 instructions never
        // touch r5") and concluded the parameter was fabricated -- but an unread argument is not
        // an absent one, and THIS call site is the proof: the console emits `mr r5, r29` at
        // 0x8264565C, loading r29 fresh from an incoming argument slot at 0x82645638. The PS3
        // DWARF types the pair (IOBufferStack*, const VehicleInputInterface*). Nothing
        // behavioural changes -- the callee still ignores it -- but the declaration stops
        // claiming something the image contradicts.
        EndVehicleTractionLineTests(lpInputBufferStack, lpInputInterface);   // r4=stack, r5=input
        CgsDev::PerfMonCpu::StopMonitor(gs_iTractionLTsPM);

        // ---- STAGE: fatal crashes (dword_82F2A178) -------------------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iCrashFatalPM);

        // The handbrake-turn crash test (asm 0x82645680..0x826457FC): the player car is
        // "sliding, not driving" when faster than 20 with its velocity more than 60 deg
        // off its nose -- force the crash. As shipped, the player index is read with NO
        // != -1 guard on this path (the gate bool is only ever set with a live player).
        if (mbCrashOnHandbrakeTurn)
        {
            const s32 liPlayer = mePlayerActiveRaceCarIndex;
            RaceCarPhysics& lrPlayer = maRaceCarVehicles[liPlayer];

            const Vector3 lCarVel = lrPlayer.GetLinearVelocity();                 // RCP+0x50
            const Vector3 lCarDir = lrPlayer.GetTransform().zAxis;                // RCP+0x30

            // vpu::Magnitude carries the console's zero-length guard (vsel on |v|^2 == 0);
            // the vendor rwmath de-models the VecFloat broadcast returns to scalar f32.
            if (vpu::Magnitude(lCarVel) > KF_HANDBRAKE_CRASH_MIN_SPEED)
            {
                if (vpu::Dot(vpu::Normalize(lCarDir),
                             vpu::Normalize(lCarVel))
                        < KF_HANDBRAKE_CRASH_MAX_COS_DOT)
                {
                    ForceRaceCarCrash(lpRequestOutputInterface,
                                      lpVehicleManagerOutputInterface,
                                      lpVehicleOutputInterface, lpDeformationInterface,
                                      static_cast<EActiveRaceCarIndex>(liPlayer));
                }
            }
        }

        if (mbCrashRaceCarWhenFatal)                               // lbzx @+172452
        {
            CrashFatalRaceCars(lpRequestOutputInterface, lpVehicleManagerOutputInterface,
                               lpVehicleOutputInterface, lpDeformationInterface,
                               lWorldEntityId);
        }

        CgsDev::PerfMonCpu::StopMonitor(gs_iCrashFatalPM);

        // ---- STAGE: the per-car update loop (dword_82F2A17C) ---------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iUpdateRaceCarsPM);

        // Player car handed to the AI (demo/autopilot): drop any live slam/shunt effect
        // (asm 0x82645844..0x826458AC -- the same partial clear VehiclePhysics::Reset's
        // recipe documents: slam scalars zeroed, slam number -1; shunt direction+speed
        // zeroed, Life = -1.0f, SpeedIncreaseToQuit = 0).
        if (maRaceCarDrivers[mePlayerActiveRaceCarIndex].meDriverType == E_DRIVER_TYPE_AI)
        {
            RaceCarPhysics& lrPlayer = maRaceCarVehicles[mePlayerActiveRaceCarIndex];
            lrPlayer.mSlamEffect.mfSteering         = 0.0f;    // +0x1114
            lrPlayer.mSlamEffect.mfOriginalSteering = 0.0f;    // +0x1118
            lrPlayer.mSlamEffect.mfSlamLife         = 0.0f;    // +0x111C
            lrPlayer.mSlamEffect.mfTotalSlamTime    = 0.0f;    // +0x1120
            lrPlayer.mSlamEffect.mi8SlamNumber      = -1;      // stb @+0x1128
            lrPlayer.mShuntEffect.mDirectionPlusDesiredSpeed  = Vector3Plus{ 0.0f, 0.0f, 0.0f, 0.0f }; // stvx 0 @+0x1130
            lrPlayer.mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x = -1.0f;  // vrlimi 8 @+0x1140
            lrPlayer.mShuntEffect.mv4_Life_SpeedIncreaseToQuit.y = 0.0f;   // vrlimi 4 @+0x1140
        }

        // Freeze the player car while the update set says "menus own the frame" (bits
        // 0x200|0x800) or the debug override is latched (asm 0x826458B0..0x826458EC).
        if (mePlayerActiveRaceCarIndex != -1)
        {
            const bool lbForceFreezePlayerCar =
                (lUpdateSet & 0xA00) != 0 || gs_bOverrideForceFrozen;
            maRaceCarVehicles[mePlayerActiveRaceCarIndex].SetForceFrozen(lbForceFreezePlayerCar);
        }

        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar != CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            // r25: player-and-additive flag, computed once per car (asm 0x826459A8..D4).
            const bool lbIsPlayerAftertouchAdditive =
                mbAftertouchIsForceAdditive && (liCar == mePlayerActiveRaceCarIndex);

            // -- driver tick (dword_82F2A180) --
            CgsDev::PerfMonCpu::StartMonitor(gs_iUpdateDriversPM);
            maRaceCarDrivers[liCar].UpdateVehicle(&maRaceCarVehicles[liCar]);
            CgsDev::PerfMonCpu::StopMonitor(gs_iUpdateDriversPM);

            // -- car physics tick (dword_82F2A184): the virtual slot +0xC dispatch into
            //    RaceCarPhysics::Update -- THE seam into the 54 force leaves. Args are the
            //    DWARF signature's (VehiclePhysics.h:1084), decoded off the call site
            //    (asm 0x82645A10..0x82645A5C):
            //      camera matrix   = &mCameraMatrix (r4 = this+172240)
            //      controls        = &maRaceCarDrivers[car].mControls (r5, the +0x40 record)
            //      impact time     = mbImpactTime (r6 = lbz this+172304)
            //      aftertouch add  = r25 (player && mbAftertouchIsForceAdditive)
            //      showtime        = (meShowtimeBehaviour == 2) (cntlzw/extrwi idiom, r8)
            //      random          = mRandom (r9 = this+16)
            //      v1/v2           = splat(lfSimTimerTimeStep) / splat(lfGameTimerTimeStep)
            CgsDev::PerfMonCpu::StartMonitor(gs_iUpdateVehiclesPM);
            maRaceCarVehicles[liCar].Update(
                VecFloat{ lfSimTimerTimeStep, lfSimTimerTimeStep,
                          lfSimTimerTimeStep, lfSimTimerTimeStep },
                VecFloat{ lfGameTimerTimeStep, lfGameTimerTimeStep,
                          lfGameTimerTimeStep, lfGameTimerTimeStep },
                &mCameraMatrix,
                maRaceCarDrivers[liCar].GetControls(),
                mbImpactTime,
                lbIsPlayerAftertouchAdditive,
                meShowtimeBehaviour == 2u,
                mRandom);
            CgsDev::PerfMonCpu::StopMonitor(gs_iUpdateVehiclesPM);

            // Per-car debug component tick (asm 0x82645A68..78; f1 = lfSimTimerTimeStep).
            // FLAG (span cast, deliberate): maRaceCarDebugComponent is the opaque 8x1024
            // span (console DebugComponent is 1024B, host is not) -- same sanctioned cast
            // seam as Construct's mpDebugComponent store; see BrnVehicleManager.h.
            reinterpret_cast<DebugComponent*>(&maRaceCarDebugComponent[liCar][0])
                ->Update(lfSimTimerTimeStep);

            // -- rigid-body change on RESET (dword_82F2A188) --
            CgsDev::PerfMonCpu::StartMonitor(gs_iRBChangePM);
            if (maRaceCarDrivers[liCar].mControls.mbReset)         // lbz +0x79(record)
            {
                // SHIPPED-DEAD COMPUTATION, reconstructed as shipped and FLAGGED, not
                // "fixed": the console recomputes the car's inverse box inertia here (asm
                // 0x82645A90..0x82645BD4 -- extent = (mHalfExtent + |COM offset|) * 2, the
                // m/12 box formula, three reciprocal-refined lanes assembled into v127) --
                // and then NEITHER STORES NOR PASSES the result. v127 is callee-saved and
                // is restored in the epilogue; no memory write exists. The DWARF hint block
                // (BrnVehicleManager.cpp:2618-2622 lInertia/lInertialTensor/lBoxExtent/
                // lfMass/lfOne + operator/) confirms these were LOCALS. The consuming call
                // was evidently compiled out of the retail build. Locals only, same as the
                // console; the PC compiler is free to fold them away -- byte-effect nil
                // either way.
                RaceCarPhysics& lrCar = maRaceCarVehicles[liCar];
                const Vector3 lBoxExtent = vpu::Mult(
                    vpu::Add(lrCar.GetHalfExtent(),
                             vpu::Abs(lrCar.mpAttribs->mBaseAttribs.mCOMOffset)),  // attribs+0x20
                    2.0f);                                                          // flt_82001D9C
                const f32 lfMassOverTwelve =
                    lrCar.GetMass().x   /* VecFloat == Vector4 splat; lane 0 */ * (1.0f / 12.0f);                // flt_82094724
                const Vector3 lInertialTensor = Vector3{                            // lfOne / (m/12 * (b^2+c^2), ...)
                    1.0f / (lfMassOverTwelve * (lBoxExtent.y * lBoxExtent.y + lBoxExtent.z * lBoxExtent.z)),
                    1.0f / (lfMassOverTwelve * (lBoxExtent.x * lBoxExtent.x + lBoxExtent.z * lBoxExtent.z)),
                    1.0f / (lfMassOverTwelve * (lBoxExtent.x * lBoxExtent.x + lBoxExtent.y * lBoxExtent.y)),
                    0.0f };
                (void)lInertialTensor;   // v127 -- never stored, never passed (see FLAG above)
            }
            CgsDev::PerfMonCpu::StopMonitor(gs_iRBChangePM);

            // -- aftertouch events (dword_82F2A18C) --
            CgsDev::PerfMonCpu::StartMonitor(gs_iAfterTouchPM);
            if (lbIsPlayerAftertouchAdditive)
            {
                ProcessAftertouchEvents(liCar,
                                        lpVehicleOutputInterface->GetGameEventQueue());  // veh+0x65F0
            }
            CgsDev::PerfMonCpu::StopMonitor(gs_iAfterTouchPM);
        }

        CgsDev::PerfMonCpu::StopMonitor(gs_iUpdateRaceCarsPM);

        // ---- [T5-ram] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. -----------------
        // Publish the player car's pose for the traffic-side probe, which runs inside
        // PhysicalTrafficManager::UpdateTrafficPhysics and has no race car in scope.
        if (gT5RamFramesLeft > 0 && mePlayerActiveRaceCarIndex >= 0)
        {
            const RaceCarPhysics& lrPlayer = maRaceCarVehicles[mePlayerActiveRaceCarIndex];
            const Vector3 lvPos = lrPlayer.GetTransform().wAxis;
            const Vector3 lvVel = lrPlayer.GetLinearVelocity();
            gT5PlayerPos[0] = lvPos.x; gT5PlayerPos[1] = lvPos.y; gT5PlayerPos[2] = lvPos.z;
            gT5PlayerVel[0] = lvVel.x; gT5PlayerVel[1] = lvVel.y; gT5PlayerVel[2] = lvVel.z;
        }

        // ---- STAGE: traffic physics (dword_82F2A190) -----------------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iUpdateTrafficPM);
        mPhysicalTrafficManager.UpdateTrafficPhysics(
            lfSimTimerTimeStep, lfGameTimerTimeStep,               // f1 / f2
            &mCameraMatrix,                                        // r6 = this+172240
            mbImpactTime,                                          // r7 = lbzx this+172304
            false);                                                // r8 = 0
        CgsDev::PerfMonCpu::StopMonitor(gs_iUpdateTrafficPM);

        // ---- STAGE: aggressive driving (dword_82F2A194) --------------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iUpdateAggressiveDrivingPM);
        UpdateAggressiveDriving(lfSimTimerTimeStep, lpRequestOutputInterface,
                                lpVehicleManagerOutputInterface, lpVehicleOutputInterface,
                                lpDeformationInterface);
        CgsDev::PerfMonCpu::StopMonitor(gs_iUpdateAggressiveDrivingPM);

        // ---- STAGE: crashes (dword_82F2A198) -------------------------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iUpdateCrashesPM);
        UpdateCrashes(lfSimTimerTimeStep);
        CgsDev::PerfMonCpu::StopMonitor(gs_iUpdateCrashesPM);

        // ---- STAGE: traffic pass-bys (dword_82F2A19C) ----------------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iUpdatePassBysPM);
        // v1 = the player car's position row (lvx this+5216*player+0x780 == RCP+0x40 ==
        // mTransform.wAxis). As shipped: no player != -1 guard (index -1 reads the tail
        // of the drivers array on the console; reconstructed verbatim).
        mPhysicalTrafficManager.PassNearbyCrashingTrafficIdsToRaceCarModule(
            lpVehicleManagerOutputInterface,
            Vector3{ maRaceCarVehicles[mePlayerActiveRaceCarIndex].GetTransform().wAxis.x,
                     maRaceCarVehicles[mePlayerActiveRaceCarIndex].GetTransform().wAxis.y,
                     maRaceCarVehicles[mePlayerActiveRaceCarIndex].GetTransform().wAxis.z,
                     0.0f });
        CgsDev::PerfMonCpu::StopMonitor(gs_iUpdatePassBysPM);

        // ---- the stationary (car-select) wheel pose (asm 0x82645EDC..0x82645FA0) -------
        // SteeringAngle.x = -MaxSteeringAngle * scale * DegToRad, where scale is -1.3 for
        // mode 0 (wheels turned one way), +1.3 (rodata KF_WHEEL_ANGLE_IN_CAR_SELECT_SCALE)
        // for mode 1 (the other way), and mode 2 (the Construct default) leaves the wheels
        // alone. The lane write is VehiclePhysics::OverrideWheelAngle (DWARF hint
        // OverrideWheelAngle + GetMaxSteeringAngle + SetPackedSteeringAngle + SetX).
        if (meStationaryPlayerWheelAngle == 0)
        {
            RaceCarPhysics& lrPlayer = maRaceCarVehicles[mePlayerActiveRaceCarIndex];
            const f32 lfAngle =
                -lrPlayer.GetMaxSteeringAngle().x * (-KF_WHEEL_ANGLE_IN_CAR_SELECT_SCALE) // flt_8209D0D4 == -1.3
                * KF_DEG_TO_RAD;
            lrPlayer.OverrideWheelAngle(VecFloat{ lfAngle, lfAngle, lfAngle, lfAngle });
        }
        else if (meStationaryPlayerWheelAngle == 1)
        {
            RaceCarPhysics& lrPlayer = maRaceCarVehicles[mePlayerActiveRaceCarIndex];
            const f32 lfAngle =
                -lrPlayer.GetMaxSteeringAngle().x * KF_WHEEL_ANGLE_IN_CAR_SELECT_SCALE   // flt_8208FA8C == 1.3
                * KF_DEG_TO_RAD;
            lrPlayer.OverrideWheelAngle(VecFloat{ lfAngle, lfAngle, lfAngle, lfAngle });
        }

        // ---- publish the player's wheel force-feedback spring (asm 0x82645FA4..C4) -----
        // Two-word copy RCP+0x13D0 -> manager-out+0x874; DWARF names the accessor
        // SetPlayerWheelFFSpring (BrnVehicleOutputInterface.h:256). As shipped: no
        // player != -1 guard here either.
        lpVehicleManagerOutputInterface->SetPlayerWheelFFSpring(
            maRaceCarVehicles[mePlayerActiveRaceCarIndex].mWheelFFSpring);
    }

    // ============================================================================================
    // VehicleManager::UpdateVehiclePhysicsPostSimulation  @0x826426E0
    //
    // Breaker is authoritative for this late-merge body and its StuntOffencesManager call ABI:
    // r4/r5/r6/r7/r8 are race bodies, drivers, active index, used-bitset, and game-event queue.
    // The VecFloat conversion is local to each vehicle call; the public manager ABI remains f32.
    // ============================================================================================
    void VehicleManager::UpdateVehiclePhysicsPostSimulation(
        const VehicleInputInterface* lpInputInterface,
        const CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimOutputBuffer,
        f32 lfTimeStep,
        BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue)
    {
        const u32 luPlayerIndex = static_cast<u32>(mePlayerActiveRaceCarIndex);
        CGS_ASSERT(luPlayerIndex < KI_MAX_ACTIVE_RACE_CARS, "invalid index : ");
        const bool lbPlayerCarUsed = mUsedRaceCars.IsBitSet(luPlayerIndex);

        if (lbPlayerCarUsed)
            DoPlayerTractionLineTestsPostSimulation(lpInputInterface, lfTimeStep);

        const VecFloat lvfTimeStep{lfTimeStep, lfTimeStep, lfTimeStep, lfTimeStep};
        for (s32 liRaceCar = mUsedRaceCars.GetFirstNonZeroBit();
             liRaceCar != CgsContainers::BitArray<8>::KI_INVALID_BITINDEX;
             liRaceCar = mUsedRaceCars.GetNextNonZeroBit(liRaceCar))
        {
            maRaceCarVehicles[liRaceCar].UpdatePostSimulation(lvfTimeStep);
            maRaceCarDrivers[liRaceCar].ClearSnappedThisFrame();
        }

        if (lbPlayerCarUsed)
            DoPlayerStuckLineTests(lpInputInterface);

        CgsDev::PerfMonCpu::StartMonitor(gs_iUpdateStuntOffencesPM);
        mStuntOffencesManager.Update(maRaceCarVehicles, maRaceCarDrivers,
                                     mePlayerActiveRaceCarIndex, &mUsedRaceCars,
                                     lpGameEventQueue, lfTimeStep);
        CgsDev::PerfMonCpu::StopMonitor(gs_iUpdateStuntOffencesPM);

        mPhysicalTrafficManager.UpdateTrafficPhysicsPostSimulation(lpSimOutputBuffer, lfTimeStep);
    }
}
}
