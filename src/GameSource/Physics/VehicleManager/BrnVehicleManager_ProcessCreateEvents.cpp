// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_ProcessCreateEvents.cpp
//
// VehicleManager::ProcessCreateEvents @0x82616770 (1,067 insns) -- THE CREATE DRAIN. The only
// writer anywhere in the XEX that SETS a bit in mUsedRaceCars (the tree's only other write is
// BrnVehicleManager_Construct.cpp's UnSetAll()).
//
// NEW SLICE TU, 2026-08-11. Home BrnVehicleManager.cpp is still unmounted; its four sibling arms
// live in BrnVehicleManager_MaintenanceEvents.cpp. This one is split out for a reason that is NOT
// file size -- see the mounting block below.
//
// =================================================================================================
// ⭐⭐ MOUNTING: THIS TU **IS** IN tools/build/build_game_exe.bat as of the boot-verified wave
//     (the `echo "%SRC%\GameSource\Physics\VehicleManager\BrnVehicleManager_ProcessCreateEvents.cpp"`
//     line, with the conductor's rationale in the rem block above it). This banner used to say the
//     opposite -- "**NOT** IN build_game_exe.bat ... the reason is RUNTIME, not COMPILE" -- and that
//     text is retired here; the history below is kept because it is what the mount decision turned on.
//
// It compiles because the concurrent Prepare-chain wave landed the seat while this one was being
// written: RaceCarPhysics::Prepare @0x82639CB8 (42 insns) is declared at RaceCarPhysics.h:167 and
// bodied in the MOUNTED RaceCarPhysics.cpp, and the VehiclePhysics::Prepare it forwards into is
// mounted too. (The commented-out declaration at RaceCarPhysics.h:135 is the retired park block,
// not the live declaration -- do not read it as the current state, as this banner briefly did.)
//
// ⚠️⚠️ WHY THE MOUNT WAS HELD BACK, AND WHAT IT TURNS ON NOW THAT IT IS LANDED.
// ProcessVehicleMaintenanceEvents IS LIVE on this build (PhysicsModule::PostSceneUpdate reaches it
// every frame -- a read-only probe recorded three real drains on one boot, naming race-car slots
// 0, 1 and 2). Setting a bit in mUsedRaceCars switches on FOUR already-mounted, already-called
// per-frame loops at once, and as of this wave they ARE switched on:
//     BrnVehicleManager_ReadUpdatedBodies.cpp:100     gravity + ExternalPhysicsBody::IntegrateTransform
//     BrnVehicleManager_UpdateVehiclePhysics.cpp:383  ResetAboveGroundTestResult per car
//     BrnVehicleManager_UpdateVehiclePhysics.cpp:473  RaceCarPhysics::Update -- the 54 force leaves
//     BrnVehicleManagerContactGeneration.cpp:340      vehicle contact generation
// The same probe measured `frozen=0` and `simpleValid=0` -- the car is not frozen and its attribs
// are not seated -- so the FIRST frame after the bit goes up runs all four against a car this
// function has just constructed. That is exactly why the mount waited for a commit whose gated run
// is BOOTED AND CHECKED rather than for a green compile; this is that commit.
//
// ⇒ There is no half-mount: the bit-set and the seat are one atomic leg, and sub-gating either is
//   the compensating-pair shape this project has paid for repeatedly. Do not re-introduce a gate.
//
// =================================================================================================
// ⭐ WHAT THE BODY IS, LEG BY LEG (asm @0x82616770..0x82617818; the whole loop is one iteration
// per event over mCreateRaceCarEventQueue, input + 128032).
//
//   head     copy the 0xA0-byte event to a local (four VMX matrix rows, two vectors, the scalar
//            tail), latch both resource handles into maRaceCar{,Graphics}ModelHandles[slot], four
//            asserts (:1303 owner, :1304 bound, "Race Car Index Already Used", model handle).
//   player   if the slot is the local player's, raise E_CURRENT_CAR_STATE_CAR_HAS_BEEN_RESET.
//   attribs  burnoutcarasset(event.mCarAssetAttribKey) -> its +0x158 handling RefSpec ->
//            physicsvehiclehandling -> VehicleAttribs::Construct + SetupAttribs; then latch the
//            asset key into VehicleAttribs::mAttribsKey (+0x358).
//   wheels   TransformToNewCOMSpace(COM), then -- SKIPPED ENTIRELY when
//            event.mbDisablePhysicsStateReset -- a four-iteration walk over the spec's WheelSpecs
//            capturing position + radius and accumulating the four-wheel mean into the COM.
//   inertia  ~200 insns of VMX Newton-Raphson that are, exactly, the solid-box inertia tensor.
//            Two Inertia values fall out of it: one for the SIMULATION and one for the seat.
//   sim      InAddRigidBody post (192-byte event, memcpy 0xC0 on the console).
//   seat     GetBoundingBox -> the vcall at slot +0x30 -> RaceCarPhysics::Prepare. Also skipped
//            when event.mbDisablePhysicsStateReset.
//   register DebugComponent::Register once per slot, then the INLINED mUsedRaceCars.SetBit --
//            the observable this whole campaign is named for -- plus the entity-id and
//            handling-body-id latches.
//   publish  CreateVehicleResult, AddRaceCarDeformationModel, the race-car type, the driver-control
//            clear, the log, and the network/player hide arms.
//
// =================================================================================================
// ⭐⭐ THE INERTIA BLOCK IS NOT "~200 INSNS OF VMX" ANY MORE -- IT IS THE SOLID-BOX TENSOR, and
// every constant in it was READ OUT OF THE IMAGE (targeted headless idat over the ARTIST .i64),
// not inferred from role:
//     flt_82001CC0 = 0.0f      flt_82001C98 = 1.0f       flt_82001D9C = 2.0f
//     flt_82001DA0 = 0.5f      flt_8208FA0C = 4.0f       flt_82094724 = 0.0833333358f (== 1/12)
//     flt_82004884 = 1.0e-5f   flt_82004F5C = 30.0f      stru_8208F630 = 120000.0f
//     flt_82004014 = 0.1f      flt_82009B84 = 1.2f
// With those in hand the block reads straight:
//     dims          = 2 * (spec->mHandlingBodyDimensions + |attribs.mCOMOffset|)   (vandc = abs)
//     invInertia.x  = 1 / ( (mass/12) * (dims.y^2 + dims.z^2) )
//     invInertia.y  = 1 / ( (mass/12) * (dims.x^2 + dims.z^2) )
//     invInertia.z  = 1 / ( (mass/12) * (dims.x^2 + dims.y^2) )
// i.e. I = m/12 * (a^2 + b^2) per axis -- the textbook solid cuboid. The `vrefp` + two
// `vnmsubfp/vmaddfp` pairs around every one of those divides are the compiler's Newton-Raphson
// expansion of a float divide; they are DE-OPTIMISED back to `/` here, per the standing rule.
//
// ⭐⭐⭐ AND THE 48-BYTE BLOCK IT BUILDS IS EXACTLY `rw::physics::Inertia`, FIELD FOR FIELD.
// The RaceCarPhysics.h banner says "`rw::physics::Inertia` has NO type in this tree yet ... the
// parameter is spelled in the declaration below only when that type lands". ⛔ THAT IS STALE: the
// type has had a committed home since task #141 (vendor/renderware/include/rw/physics/inertia.h),
// complete with a compiled seven-member layout gate. Laying the console's stack block onto it:
//     +0x00 (16B)  the three reciprocals          -> mInvTens
//     +0x10        1 / mass                       -> mInvMass
//     +0x14        1 / min(the three reciprocals) -> mSpherical
//     +0x18        120000.0f                      -> mMaxVelocity
//     +0x1C        30.0f                          -> mMaxOmega
//     +0x20        1.0e-5f                        -> mLinearDrag
//     +0x24        1.0e-5f                        -> mAngularDrag
//     +0x28 (8B)   NEVER WRITTEN                  -> the tail padding that header documents
// and the +0x14 store is, instruction for instruction, `SetInverseInertia`'s own documented body
// (`fcmpu/blt/fmr` min-of-three then `fdivs 1.0f`). Seven of seven, including the hole.
//
// ⚠️ TWO Inertia VALUES, NOT ONE, and they differ in three fields. The console builds the block
// once, copies it to a second stack slot, and then OVERWRITES three of the copy's fields before
// handing that copy to the SIMULATION -- while the ORIGINAL goes to Prepare:
//                        to Prepare (var_920)     to InAddRigidBody (var_600)
//     mInvTens           invInertia               invInertia * 1.2f
//     mSpherical         1 / min(invInertia)      1 / min(invInertia * 1.2f)   (recomputed)
//     mLinearDrag        1.0e-5f                  0.1f
//     mAngularDrag       1.0e-5f                  0.1f
// Collapsing them into one value would be a silent physics change; both are reproduced.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"   // mCreateRaceCarEventQueue
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"  // the request + manager-output interfaces
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationInputInterface.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"
#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"                // the +0x158 handling RefSpec
#include "GameSource/AttribSys/Generated/classes/physicsvehiclehandling.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"         // GetBoundingBox out-param
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // DebugComponent::Register
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/physics/inertia.h"                                                    // rw::physics::Inertia
#include "rw/physics/rigidbody.h"                                                  // rw::physics::ACTIVE_BODY
#include "rw/math/vpu/vector3_operation.h"                                         // Add / Mult

namespace
{
    // The image constants this body reads (see the banner). Named rather than inlined so a
    // reviewer can check each against the dump, and so none of them can be mistaken for a guess.
    const f32 KF_TWO                 = 2.0f;        // flt_82001D9C
    const f32 KF_HALF                = 0.5f;        // flt_82001DA0
    const f32 KF_NUM_WHEELS          = 4.0f;        // flt_8208FA0C -- the four-wheel mean divisor
    const f32 KF_BOX_INERTIA_COEFF   = 1.0f / 12.0f;// flt_82094724 (0.0833333358f)
    const f32 KF_SEAT_DRAG           = 1.0e-5f;     // flt_82004884 -- linear AND angular, at the seat
    const f32 KF_SIM_DRAG            = 0.1f;        // flt_82004014 -- linear AND angular, in the sim
    const f32 KF_MAX_LINEAR_VELOCITY = 120000.0f;   // stru_8208F630
    const f32 KF_MAX_ANGULAR_VELOCITY= 30.0f;       // flt_82004F5C
    const f32 KF_SIM_INERTIA_SCALE   = 1.2f;        // flt_82009B84 -- the sim's softer tensor

    // `vandc v13, v13, v124` with v124 == the per-lane sign mask (`vspltisw v0,-1 ; vslw v124,v0,v0`
    // == 0x80000000 in every lane). ⚠ The console clears the sign bit in ALL FOUR lanes -- vandc is
    // a whole-register operation and knows nothing about Vector3's three-lane convention. This
    // helper deliberately PRESERVES .w instead of taking its absolute value: the .w lane is dead
    // downstream (the only consumer is the x/y/z box-dimension math at the mHandlingBodyDimensions
    // add below), so the divergence is cosmetic, not behavioural. Named here so the next reader
    // does not "fix" the helper to match the asm and assume it mattered.
    inline Vector3 AbsPerLane(Vector3 lvValue)
    {
        Vector3 lvResult;
        lvResult.x = (lvValue.x < 0.0f) ? -lvValue.x : lvValue.x;
        lvResult.y = (lvValue.y < 0.0f) ? -lvValue.y : lvValue.y;
        lvResult.z = (lvValue.z < 0.0f) ? -lvValue.z : lvValue.z;
        lvResult.w = lvValue.w;
        return lvResult;
    }
}

namespace BrnPhysics
{
namespace Vehicle
{
    void VehicleManager::ProcessCreateEvents(const VehicleInputInterface* lpInputInterface,
                                             VehicleOutputRequestInterface* lpOutputInterface,
                                             VehicleManagerOutputInterface* lpManagerOutputInterface,
                                             Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        const VehicleInputInterface::CreateRaceCarEventQueue* lpQueue =
            lpInputInterface->GetCreateRaceCarEventQueue();

        // ⚠️ The bound is re-read every iteration (`lwz r10,8(r10)` at 0x826177F4). Live test.
        for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
        {
            // The console copies the whole 0xA0-byte record to the stack before touching it
            // (0x826169B4..0x82616A58) and then reads only the copy. Reproduced by value.
            const CreateRaceCarEvent lEvent = lpQueue->GetEvent(liEvent);

            const u32 luEntityWord = static_cast<u32>(lEvent.mVolumeInstanceID.muId >> 32);
            const u32 luOwner      = luEntityWord >> 24;
            const u32 luRaceCar    = lEvent.mVolumeInstanceID.GetEntityIDEntityIndex();

            // `stdx r11,r28,r24` / `stdx r10,r9,r24` -- the two latches happen BEFORE the asserts.
            maRaceCarModelHandles[luRaceCar]         = lEvent.mModelHandle;
            maRaceCarGraphicsModelHandles[luRaceCar] = lEvent.mGraphicsHandle;

            CGS_ASSERT(luOwner == 1u,
                "lEvent.mVolumeInstanceID.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR"); // :1303
            CGS_ASSERT(luRaceCar < 8u, "leActiveRaceCarIndex < ku8MaxNumRaceCars");               // :1304
            CGS_ASSERT(!mUsedRaceCars.IsBitSet(luRaceCar), "Race Car Index Already Used");
            CGS_ASSERT(lEvent.mModelHandle.mpResourceMemory != 0,
                       "lEvent.mModelHandle.GetResource() != NULL");

            // ---- the player arm -----------------------------------------------------------
            // `lwz r11,[this+172204] ; cmpw r27,r11 ; bne` then the OR of 0x20 into the stunt
            // manager's +0x28. Both reached by name.
            if (static_cast<s32>(luRaceCar) == static_cast<s32>(mePlayerActiveRaceCarIndex))
            {
                mStuntOffencesManager.SetCurrentRaceCarState(E_CURRENT_CAR_STATE_CAR_HAS_BEEN_RESET);
            }

            // ---- the AttribSys chase ------------------------------------------------------
            // Each generated ctor below IS the console's inlined sequence, exactly as the
            // BrnSimpleVehiclePhysics.cpp:786 precedent spells it:
            //   Attrib::FindCollection(hash64("burnoutcarasset"), event.mCarAssetAttribKey)
            //     -> Attrib::Instance -> DefaultDataArea(0x228) when the collection has no layout
            //     -> RefSpec at +0x158 -> RefSpec::GetCollection -> physicsvehiclehandling
            VehicleAttribs lVehicleAttribs;
            {
                Attrib::Gen::burnoutcarasset lCarAsset(lEvent.mCarAssetAttribKey, NULL);
                Attrib::Gen::physicsvehiclehandling lHandling(
                    const_cast<Attrib::Collection*>(
                        lCarAsset.GetPhysicsVehicleHandlingRefSpec()->GetCollection()), NULL);

                lVehicleAttribs.Construct();
                // The checked copy @0x825BDB88 -- the console's by-value argument.
                Attrib::Gen::physicsvehiclehandling lHandlingCopy(lHandling);
                lVehicleAttribs.SetupAttribs(lHandlingCopy);
            }
            // `ld r10,var_860 ; std r10,var_148` == VehicleAttribs + 0x358 (mAttribsKey).
            lVehicleAttribs.mAttribsKey = lEvent.mCarAssetAttribKey;

            // ---- the deformation spec -----------------------------------------------------
            // `lwzx r11,r28,r24 ; lwz r28,0(r11)` -- the SafeResourceHandle double deref
            // (mpResourceMemory, then the SmallResource's main-memory pointer).
            CGS_ASSERT(maRaceCarModelHandles[luRaceCar].mpResourceMemory != 0,
                       "maRaceCarModelHandles[leActiveRaceCarIndex].GetResource() != NULL");
            Deformation::StreamedDeformationSpec* lpSpec =
                *reinterpret_cast<Deformation::StreamedDeformationSpec* const*>(
                     maRaceCarModelHandles[luRaceCar].mpResourceMemory);

            lpSpec->TransformToNewCOMSpace(lVehicleAttribs.mBaseAttribs.mCOMOffset);
            CGS_ASSERT(lpSpec != 0, "lpDeformationModel != NULL");

            // ---- the wheel walk (skipped when the event disables the physics-state reset) ---
            Vector3 lvHandlingBodyOffset;
            lvHandlingBodyOffset.x = 0.0f; lvHandlingBodyOffset.y = 0.0f;
            lvHandlingBodyOffset.z = 0.0f; lvHandlingBodyOffset.w = 0.0f;   // `vmr128 v125,v123(0)`

            Vector3 lavWheelPositions[4];
            f32     lafWheelRadii[4];

            if (!lEvent.mbDisablePhysicsStateReset)   // `lbz r11,var_83C ; cmplwi 0 ; bne`
            {
                for (s32 liWheel = 0; liWheel < 4; ++liWheel)
                {
                    // The console emits StreamedDeformationSpec::GetWheelSpec's own bound
                    // tripwire twice per iteration (once per member read), inlined.
                    CGS_ASSERT(liWheel < 4, "liWheel < eNumWheels");
                    lavWheelPositions[liWheel] = lpSpec->maWheelSpecs[liWheel].mPosition;

                    lvHandlingBodyOffset =
                        rw::math::vpu::Add(lvHandlingBodyOffset, lavWheelPositions[liWheel]);

                    CGS_ASSERT(liWheel < 4, "liWheel < eNumWheels");
                    // `vspltw v13,v13,1` == lane .y of maWheelSpecs[i].mScale, halved.
                    lafWheelRadii[liWheel] = KF_HALF * lpSpec->maWheelSpecs[liWheel].mScale.y;
                }

                // `v125 = v125 * (1/4)` -- the VMX reciprocal expansion of a divide by 4.0f.
                lvHandlingBodyOffset = lvHandlingBodyOffset / KF_NUM_WHEELS;

                // `vaddfp128 v0,v13,v125 ; stvx128 v0,r0,&attribs+0x20`
                lVehicleAttribs.mBaseAttribs.mCOMOffset =
                    rw::math::vpu::Add(lVehicleAttribs.mBaseAttribs.mCOMOffset, lvHandlingBodyOffset);
            }

            // ---- the solid-box inertia tensor (see the banner) -----------------------------
            const Vector3 lvBoxDimensions =
                (rw::math::vpu::Add(lpSpec->mHandlingBodyDimensions,
                                    AbsPerLane(lVehicleAttribs.mBaseAttribs.mCOMOffset))) * KF_TWO;

            // `lvx128 v0,&attribs+0x70 ; vspltw v0,v0,0` -- lane .x of the packed mass register.
            const f32 lfMass =
                lVehicleAttribs.mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;
            const f32 lfCoeff = KF_BOX_INERTIA_COEFF * lfMass;

            const f32 lfXX = lvBoxDimensions.x * lvBoxDimensions.x;
            const f32 lfYY = lvBoxDimensions.y * lvBoxDimensions.y;
            const f32 lfZZ = lvBoxDimensions.z * lvBoxDimensions.z;

            Vector3 lvInverseInertia;
            lvInverseInertia.x = 1.0f / (lfCoeff * (lfYY + lfZZ));
            lvInverseInertia.y = 1.0f / (lfCoeff * (lfXX + lfZZ));
            lvInverseInertia.z = 1.0f / (lfCoeff * (lfXX + lfYY));
            // ⚠️ Lane .w is NEVER written by the console: the three `vrlimi128` inserts touch
            // lanes 0, 1 and 2 only, so v126's .w carries whatever the previous iteration left.
            // Reproduced as an explicit zero rather than as uninitialised stack -- the lane is
            // the Vector3 pad and nothing downstream reads it.
            lvInverseInertia.w = 0.0f;

            rw::physics::Inertia lSeatInertia;
            lSeatInertia.SetLinearDrag(KF_SEAT_DRAG);            // stfs -> block +0x20
            lSeatInertia.SetAngularDrag(KF_SEAT_DRAG);           //         block +0x24
            lSeatInertia.SetMaxLinearVelocity(KF_MAX_LINEAR_VELOCITY);   // block +0x18
            lSeatInertia.SetMaxAngularVelocity(KF_MAX_ANGULAR_VELOCITY); // block +0x1C
            lSeatInertia.SetInverseMass(1.0f / lfMass);          //         block +0x10
            // Sets +0x00 AND recomputes +0x14 as 1/min(diag) -- which is exactly the console's
            // `stvx128 v126 ; fcmpu/blt/fmr min-of-three ; fdivs 1.0f ; stfs +0x14` pair.
            lSeatInertia.SetInverseInertia(lvInverseInertia);

            // ---- the simulation copy: a softer tensor and 10,000x the drag ------------------
            rw::physics::Inertia lSimInertia = lSeatInertia;
            lSimInertia.SetInverseInertia(lvInverseInertia * KF_SIM_INERTIA_SCALE);
            lSimInertia.SetLinearDrag(KF_SIM_DRAG);
            lSimInertia.SetAngularDrag(KF_SIM_DRAG);

            // ---- post the body into the simulation request channel -------------------------
            // ⚠️ Its ONLY consumer is PhysicsModule::BridgeVehicleManagerToSimulation_PostScene
            // @0x825AB408, a DELIBERATELY INERT gate, and PostSceneUpdate destroys the VehManager
            // buffer in the same call that creates it -- so on this build the post is a producer
            // into a buffer nothing drains, BY THE CONSOLE'S OWN DESIGN. Reproduced as shipped.
            {
                CgsPhysics::PhysicsSimulationIO::InAddRigidBody lAddBody;
                // `clrlwi r11,r8,8 ; oris r11,r11,0xB00 ; sldi r11,r11,32` -- the entity word with
                // its owner byte replaced by 0x0B (E_ENTITYTYPE_PROP_COLLISION_RACECAR), promoted
                // into the handle's high dword. The symmetric partner of ProcessRemoveEvents'.
                lAddBody.mID = static_cast<u64>((luEntityWord & 0x00FFFFFFu) | 0x0B000000u) << 32;
                lAddBody.mRigidBody.mTransform       = lEvent.mInitialTransform;
                lAddBody.mRigidBody.mVelocity        = lEvent.mInitialVelocity;
                lAddBody.mRigidBody.mAngularVelocity = lEvent.mAngularVelocity;
                lAddBody.mRigidBody.mInertia         = lSimInertia;
                lAddBody.mRigidBody.mbSpy            = true;    // `li r29,1 ; stb r29,var_680`
                lAddBody.meState = rw::physics::ACTIVE_BODY;    // `li r11,4 ; stw r11,var_670`
                lpOutputInterface->GetRequiredRigidBodiesQueue()->AddEvent(lAddBody);
            }

            // ---- the seat ------------------------------------------------------------------
            CgsGeometric::AxisAlignedBox lAABB;
            lpSpec->GetBoundingBox(lAABB);

            // ⛔ THE ODR FORK RaceCarPhysics.h FLAGGED IS LIVE AT THIS ONE LINE, AND IT IS THE
            // ONLY PLACE IN THE TREE WHERE THE TWO SPELLINGS OF THIS BOX MEET.
            // The console hands the SAME 32 bytes straight from GetBoundingBox's out-param to
            // Prepare's r5 (`addi r4,r1,var_760 ; bl GetBoundingBox` then `addi r5,r1,var_760`).
            // In this tree those 32 bytes have two names:
            //     CgsGeometric::AxisAlignedBox        { Vector4 mMin; Vector4 mMax; }  <- what
            //         StreamedDeformationSpec::GetBoundingBox fills (the PS3 mangle's spelling)
            //     BrnPhysics::Vehicle::AxisAlignedBox { Vector3 mMin; Vector3 mMax; }  <- what
            //         VehiclePhysics::Prepare / SimpleVehiclePhysics::Prepare declare
            // Same size, same two fields, same lane widths -- but they are DIFFERENT TYPES, and a
            // reinterpret_cast here would be exactly the silent-fork trade RaceCarPhysics.h's
            // banner warns about. Converted BY NAMED FIELD instead, so the day either side changes
            // shape this line fails to compile rather than mis-reading a corner.
            // DELETE-WHEN the geometry group re-homes the in-namespace slice onto
            // CgsGeometric::AxisAlignedBox.
            AxisAlignedBox lSeatAABB;
            lSeatAABB.mMin.x = lAABB.mMin.x; lSeatAABB.mMin.y = lAABB.mMin.y;
            lSeatAABB.mMin.z = lAABB.mMin.z; lSeatAABB.mMin.w = lAABB.mMin.w;
            lSeatAABB.mMax.x = lAABB.mMax.x; lSeatAABB.mMax.y = lAABB.mMax.y;
            lSeatAABB.mMax.z = lAABB.mMax.z; lSeatAABB.mMax.w = lAABB.mMax.w;

            if (!lEvent.mbDisablePhysicsStateReset)
            {
                // ⛔ THE VCALL AT CONSOLE VTABLE SLOT +0x30 (`lwz r11,0(r3) ; lwz r11,0x30(r11) ;
                // mtctr ; bctrl` @0x82617224..0x8261724C) == RaceCarPhysics::Prepare @0x82639CB8.
                // Eleven arguments, every one of them identified at the call site:
                //   r4 = &lEvent.mInitialTransform     v1 = lEvent.mInitialVelocity
                //   v2 = lEvent.mAngularVelocity       v3 = lvHandlingBodyOffset (the wheel mean)
                //   v4 = lpSpec->mHandlingBodyDimensions
                //   r5 = the AABB GetBoundingBox filled one instruction earlier
                //   r6..r10 + stack@0x98 = lSeatInertia BY VALUE (six doublewords == 48 bytes)
                //   stack@0xA4/0xAC/0xB4 = &lVehicleAttribs / lavWheelPositions / lafWheelRadii
                //   stack@0xBF (a byte)  = lEvent.miCarStrengthStat
                // Spelled as a plain member call: this tree reaches members BY NAME, never by
                // console vtable slot (the host slot numbering is the compiler's).
                // ⚠️ The AABB parameter is spelled CgsGeometric::AxisAlignedBox here because that
                // is what StreamedDeformationSpec::GetBoundingBox fills. RaceCarPhysics.h's own
                // banner flags that VehiclePhysics::Prepare's declaration spells it with the
                // in-namespace BrnPhysics::Vehicle::AxisAlignedBox slice instead -- the same 32
                // bytes and the same two fields, but a live ODR fork. Re-homing the slice onto
                // CgsGeometric::AxisAlignedBox is a geometry-group change; if the declaration
                // still carries the slice when it lands, this argument needs the conversion, not
                // a cast.
                maRaceCarVehicles[luRaceCar].Prepare(
                    lEvent.mInitialTransform,
                    lEvent.mInitialVelocity,
                    lEvent.mAngularVelocity,
                    lvHandlingBodyOffset,
                    lpSpec->mHandlingBodyDimensions,
                    lSeatAABB,
                    lSeatInertia,
                    &lVehicleAttribs,
                    lavWheelPositions,
                    lafWheelRadii,
                    static_cast<u8>(lEvent.miCarStrengthStat));
            }

            // ---- register the per-car debug component, once ---------------------------------
            if (!mabRaceCarDebugComponentRegistered[luRaceCar])
            {
                // `slwi r11,r27,10 ; add r11,r11,r24 ; addis r3,r11,2 ; addi r3,r3,0x7DC0`
                // == this + 163264 + 1024*slot == &maRaceCarDebugComponent[slot]. The array is an
                // opaque 8x1024 span (the real component is another group's type), so the cast is
                // the same sanctioned span-cast seam VehicleManager::Construct already uses.
                CgsDev::DebugComponent* lpComponent = reinterpret_cast<CgsDev::DebugComponent*>(
                    &maRaceCarDebugComponent[luRaceCar][0]);

                // ⛔ [FLAG PC bring-up] NULL-VPTR GATE (conductor, 2026-08-11, crash-measured on
                // the FIRST live create event this project ever drained): the console constructs
                // the per-car debug components in VehicleManager's ctor chain; this build's chain
                // is gated, so the span is zero storage and Register()'s first virtual dispatch
                // reads vtable slot +0x28 through a NULL vptr -- AV at DebugComponent::Register
                // +0x2F, process down (BrnCrash.png, module+0xC993F). The gate tests the vptr
                // word itself; the registered flag stays FALSE on skip so the slot re-registers
                // the moment the real component ctor lands.
                // DELETE-WHEN VehicleManager's debug-component construction is reconstructed.
                if (*reinterpret_cast<void* const*>(lpComponent) != 0)
                {
                    lpComponent->Register();
                    mabRaceCarDebugComponentRegistered[luRaceCar] = true;
                }
                else
                {
                    static bool sbReportedNullDebugComponentVptr = false;
                    if (!sbReportedNullDebugComponentVptr)
                    {
                        sbReportedNullDebugComponentVptr = true;
                        if (CgsDev::Message::gxMessageFilterFlags & 1)
                            *CgsDev::Log::gpDebugPrint
                                << "[FLAG PC bring-up] ProcessCreateEvents: race-car debug "
                                   "component vptr is NULL (ctor chain gated) -- Register() "
                                   "skipped, reported once\n";
                    }
                }
            }

            // ---- ⭐⭐⭐ THE BIT ---------------------------------------------------------------
            // The four latches the console emits as one interleaved block (0x826173B8..0x826173E0):
            // the result event, the live-car bit, the entity id and the handling-body id.
            CGS_ASSERT(luRaceCar < 8u, "Index: liRaceCar, Number of bits: 8");   // CgsBitArray.h:222

            CreateVehicleResult lResult;
            lResult.mVolumeInstanceID = lEvent.mVolumeInstanceID;
            lResult.mbSuccess         = true;                        // `li r9,1 ; stb r9,var_818`

            mUsedRaceCars.SetBit(luRaceCar);
            maRaceCarEntityIDs[luRaceCar].muValue = luEntityWord;
            // `sldi r26,r8,32` -- the entity word promoted into the handle's HIGH dword, low dword
            // zero. This is what makes the 4-byte `::RigidBodyId` stand-in the deformation events
            // use lossless; see BrnVehicleManager_MaintenanceEvents.cpp's banner.
            maRaceCarHandlingBodyIDs[luRaceCar] = static_cast<u64>(luEntityWord) << 32;

            lpManagerOutputInterface->AddCreateVehicleResult(lResult);

            AddRaceCarDeformationModel(lpDeformationInterface,
                                       static_cast<s32>(luRaceCar),
                                       lEvent.mInitialTransform,
                                       lEvent.mfDeformAmount,
                                       lEvent.meBaseDeformationType);

            maeRaceCarTypes[luRaceCar] = lEvent.meRaceCarType;   // `stwx r10,r28,r24`

            // ---- the driver-control clear ---------------------------------------------------
            // Byte for byte the SAME inlined VehicleDriver::ClearControls as ProcessRemoveEvents
            // (0x82617420..0x8261748C vs 0x826163D4..0x82616444): the identical store set with the
            // identical two omissions (+0x3A mbToggle, +0x44 meDriverType).
            // ⭐ DELETE-WHEN HONOURED 2026-08-11 (merge of the two create-drain waves):
            // VehicleDriver::ClearControls now HAS a body (BrnVehicleDriver.cpp), recovered from
            // these two inline sites together. The 28-store block that stood open here was checked
            // field-for-field against it before the swap -- identical, omissions included -- and the
            // call sits exactly where the console's inlined run did.
            maRaceCarDrivers[luRaceCar].ClearControls();

            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint << "HIDE_ONLINE: " << "Created race car "
                    << static_cast<s32>(luRaceCar) << ", type "
                    << static_cast<s32>(maeRaceCarTypes[luRaceCar]) << "\n";
            }

            // ---- the two hide arms ----------------------------------------------------------
            if (lEvent.meRaceCarType == BrnWorld::E_RACE_CAR_TYPE_NETWORK)
            {
                // `lis/ori r10,0x2A11E ; lbzx r10,r24,r10 ; cmplwi 0 ; bne` -- 0x2A11E == 172318
                // == mbInOnlineGameModeStartLine. A network car created while the online mode is
                // sitting on the start line is NOT hidden.
                if (!mbInOnlineGameModeStartLine)
                {
                    CGS_ASSERT(luRaceCar < 8u, "luIndex < NUMBITS");     // CgsBitArray.h:241
                    mNetworkCarsRecievedFirstUpdate.UnSetBit(luRaceCar); // this + 44728

                    if (CgsDev::Message::gxMessageFilterFlags & 1)
                    {
                        *CgsDev::Log::gpDebugPrint << "HIDE_ONLINE: " << "Making network race car "
                            << static_cast<s32>(luRaceCar) << ", type "
                            << static_cast<s32>(maeRaceCarTypes[luRaceCar])
                            << " hidden because it was just created\n";
                    }
                    SetNetworkRaceCarHidden(static_cast<EActiveRaceCarIndex>(luRaceCar), 1);
                }
            }
            else if (static_cast<s32>(luRaceCar) == static_cast<s32>(mePlayerActiveRaceCarIndex))
            {
                // ⚠️ The console prints mePlayerActiveRaceCarIndex here, NOT the loop's slot
                // (`lwz r10,var_9AC ; lwz r5,0(r10)` @0x82617768). They are equal on this branch,
                // so the difference is invisible -- reproduced as written rather than "tidied".
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint << "HIDE_ONLINE: "
                        << "Making all network race cars hidden for at least 1 frame because player car "
                        << static_cast<s32>(mePlayerActiveRaceCarIndex) << " was just created\n";
                }
                SetAllNetworkRaceCarsHidden(1);
            }

            // The two Attrib::Instance destructors at 0x826177D8/0x826177E4 are the automatic
            // destruction of lHandling and lCarAsset; the scope above is what expresses them.
        }
    }
}
}
