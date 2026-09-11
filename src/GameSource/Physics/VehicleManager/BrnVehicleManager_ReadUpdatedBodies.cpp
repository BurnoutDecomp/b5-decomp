// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_ReadUpdatedBodies.cpp
//
// THE PER-FRAME GRAVITY + INTEGRATION STEP, for race cars AND traffic.
//   VehicleManager::ReadUpdatedBodies          @0x82619A10 (198 insns)
//   PhysicalTrafficManager::ReadUpdatedBodies  @0x825EF608 (334 insns)
//
// Both were conductor gates / absent; both are real here, and the gate for the first was deleted
// in the same commit (LNK2005 is the intended tripwire if it comes back).
//
// THE NAME IS A LIE AND THE COMMITTED HEADER COMMENT WAS WRONG. "ReadUpdatedBodies" reads
// NOTHING back from the simulation. The X360 body takes the OutUpdateRigidBody queue in r4,
// stashes it at var_1C, and NEVER DEREFERENCES IT -- it only forwards it, unread, to the traffic
// manager's twin, where it is consumed by a DEBUG DUPLICATE CHECK and nothing else. The header's
// old claim ("then drain the sim's OutUpdateRigidBody queue into the owning cars") describes code
// that does not exist in either body, and its other clause ("live cars NOT owned by the sim this
// frame") describes an ownership test the asm does not make -- the only guard is mbFrozen. Both
// are corrected at the declaration.
//
// What these two functions ACTUALLY are, and why that matters more than the name: a Burnout race
// car is a BrnPhysics::ExternalPhysicsBody -- the game integrates it ITSELF and only publishes the
// pose to rw::physics. rw::physics' own SimulationParams::mGravity never touches it, and
// ExternalPhysicsBody::CalculateNewVelocity loads no constants at all. So THIS is the only place
// gravity enters a car, and the only place a car's pose advances. With this gated, a body added to
// the simulation would have sat perfectly still forever. (The identification of the constant is
// banked at BrnVehicleConstants.h:10-46, [V] 2026-08-03; this wave re-derived the same reading
// independently from the asm before writing a line.)
//
// AS-SHIPPED, NAMED HONESTLY: the y-lane subtraction below is a raw Euler velocity step with no
// ground reaction of any kind, because the traction-line family that produces the road contact
// (StartVehicleTractionLineTests / EndVehicleTractionLineTests / ReadRaceCarTractionLineTest-
// Results) is still gated. That is the console's own arithmetic, not a simplification -- but it
// means a body in the sim TODAY would accelerate downward unopposed. The create path is
// deliberately NOT enabled in this commit for exactly that reason.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"      // KF_GRAVITY
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // gpDebugPrint ([ortho] witness only)
#include "rw/math/vpu/vector3_operation.h"                              // Dot / Cross / MagnitudeSquared
#include <cmath>                                                        // std::sqrt, std::fabs
#include <cstdlib>                                                      // getenv ([ortho] witness only)

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // ==========================================================================================
    // [ortho] -- NOT IN THE X360 BINARY. OPT-IN (BRN_ORTHO_PROBE=1). Read-only: it reads the
    // car's own transform after the console's integrate step and prints. It changes nothing.
    //
    // ⭐ WHAT IT MEASURES AND WHY. ExternalPhysicsBody::IntegrateTransform @0x825A7930 advances
    // each basis row by the FIRST-ORDER step `row += (omega*dt) x row` and then repairs the basis
    // with rw::math::vpu::OrthoNormalize3x3 @0x82203B28. The first-order step leaves the rows
    // longer AND no longer mutually perpendicular: for rows a,b the new dot is exactly
    // (omega x a).(omega x b) dt^2, whose x/y term is -omega.x*omega.y*dt^2. So the SHEAR a step
    // introduces is proportional to the PRODUCT OF TWO DIFFERENT ANGULAR-VELOCITY COMPONENTS --
    // it is identically zero for the single-axis yaw of ordinary driving and largest for a car
    // tumbling about a general axis, i.e. IN THE AIR.
    //
    // The two numbers, both scale-free so a row length cannot disguise them:
    //   maxdot = max over the three row pairs of |a.b| / (|a||b|)   -- 0 when perpendicular.
    //   det    = a.(b x c) / (|a||b||c|)                            -- 1 when right-handed
    //            orthonormal, 0 when the basis has COLLAPSED ONTO A PLANE (the flattened car).
    //
    // PRE-REGISTERED, before any run: the console re-orthonormalises every step, so on a faithful
    // build maxdot must stay in the first histogram band (< 1e-3) on EVERY frame, airborne or not,
    // and det must stay above 0.999. Anything that leaves those bands is an accumulation the
    // console does not have.
    //
    // ⛔ THE CONTROL BITES OR THE PROBE IS WORTHLESS. On arming it prints the same two metrics for
    // a matrix that IS orthonormal (identity) and for one whose shear is known in closed form
    // (x=(1,0,0), y=normalized(0.2,1,0), z=(0,0,1) -> maxdot 0.196116, det 0.980581). If the
    // control line does not read 0/1 then 0.196116/0.980581, every number below is void.
    // DELETE-WHEN issue #15 is closed and banked.
    // ==========================================================================================
    struct OrthoMetric { f32 mfMaxDot; f32 mfDet; f32 mfLenX; f32 mfLenY; f32 mfLenZ; };

    inline OrthoMetric MeasureOrtho(const Vector3& lrX, const Vector3& lrY, const Vector3& lrZ)
    {
        namespace vpu = rw::math::vpu;
        OrthoMetric lM;
        lM.mfLenX = std::sqrt(vpu::MagnitudeSquared(lrX));
        lM.mfLenY = std::sqrt(vpu::MagnitudeSquared(lrY));
        lM.mfLenZ = std::sqrt(vpu::MagnitudeSquared(lrZ));

        const f32 lfDenXY = lM.mfLenX * lM.mfLenY;
        const f32 lfDenYZ = lM.mfLenY * lM.mfLenZ;
        const f32 lfDenZX = lM.mfLenZ * lM.mfLenX;
        const f32 lfXY = (lfDenXY > 0.0f) ? std::fabs(vpu::Dot(lrX, lrY)) / lfDenXY : 0.0f;
        const f32 lfYZ = (lfDenYZ > 0.0f) ? std::fabs(vpu::Dot(lrY, lrZ)) / lfDenYZ : 0.0f;
        const f32 lfZX = (lfDenZX > 0.0f) ? std::fabs(vpu::Dot(lrZ, lrX)) / lfDenZX : 0.0f;
        lM.mfMaxDot = (lfXY > lfYZ) ? lfXY : lfYZ;
        if (lfZX > lM.mfMaxDot) { lM.mfMaxDot = lfZX; }

        const f32 lfDen3 = lM.mfLenX * lM.mfLenY * lM.mfLenZ;
        lM.mfDet = (lfDen3 > 0.0f) ? (vpu::Dot(lrX, vpu::Cross(lrY, lrZ)) / lfDen3) : 0.0f;
        return lM;
    }

    // maxdot bands: [0,1e-4) [1e-4,1e-3) [1e-3,1e-2) [1e-2,0.1) [0.1,0.3) [0.3,0.6) [0.6,1]
    inline s32 OrthoDotBand(f32 lfV)
    {
        if (lfV < 1.0e-4f) { return 0; }
        if (lfV < 1.0e-3f) { return 1; }
        if (lfV < 1.0e-2f) { return 2; }
        if (lfV < 0.1f)    { return 3; }
        if (lfV < 0.3f)    { return 4; }
        if (lfV < 0.6f)    { return 5; }
        return 6;
    }

    // det bands: (0.999,inf) (0.99,0.999] (0.9,0.99] (0.5,0.9] (0.1,0.5] (0.01,0.1] [-inf,0.01]
    inline s32 OrthoDetBand(f32 lfV)
    {
        if (lfV > 0.999f) { return 0; }
        if (lfV > 0.99f)  { return 1; }
        if (lfV > 0.9f)   { return 2; }
        if (lfV > 0.5f)   { return 3; }
        if (lfV > 0.1f)   { return 4; }
        if (lfV > 0.01f)  { return 5; }
        return 6;
    }

    void OrthoProbe(RaceCarPhysics& lrCar, s32 liSlot)
    {
        static s32 siArmed = -1;
        if (siArmed < 0)
        {
            const char* lpcEnv = getenv("BRN_ORTHO_PROBE");
            siArmed = (lpcEnv != 0 && lpcEnv[0] != '0') ? 1 : 0;
        }
        if (siArmed != 1 || CgsDev::Log::gpDebugPrint == 0) { return; }

        static bool sbControlPrinted = false;
        if (!sbControlPrinted)
        {
            sbControlPrinted = true;
            const f32 lfInv = 1.0f / std::sqrt(1.04f);
            const OrthoMetric lGood = MeasureOrtho(Vector3{ 1.0f, 0.0f, 0.0f, 0.0f },
                                                   Vector3{ 0.0f, 1.0f, 0.0f, 0.0f },
                                                   Vector3{ 0.0f, 0.0f, 1.0f, 0.0f });
            const OrthoMetric lBad  = MeasureOrtho(Vector3{ 1.0f, 0.0f, 0.0f, 0.0f },
                                                   Vector3{ 0.2f * lfInv, lfInv, 0.0f, 0.0f },
                                                   Vector3{ 0.0f, 0.0f, 1.0f, 0.0f });
            *CgsDev::Log::gpDebugPrint
                << "[ortho] CONTROL identity maxdot " << lGood.mfMaxDot << " det " << lGood.mfDet
                << " | sheared maxdot " << lBad.mfMaxDot << " det " << lBad.mfDet
                << "  (expect 0 / 1 then 0.196116 / 0.980581)\n";
        }

        static u32 suFrame = 0u;
        static u32 suDotBand[7] = { 0u, 0u, 0u, 0u, 0u, 0u, 0u };
        static u32 suDetBand[7] = { 0u, 0u, 0u, 0u, 0u, 0u, 0u };
        static u32 suAirFrames = 0u;
        static f32 sfMaxDotEver = 0.0f, sfMinDetEver = 2.0f;
        static u32 suMaxDotFrame = 0u, suMinDetFrame = 0u;
        static u32 suLines = 0u;

        if (liSlot != 0) { return; }   // slot 0 is the player car in every harness recipe
        ++suFrame;

        const Matrix44Affine& lrT = lrCar.GetTransform();
        const OrthoMetric lM = MeasureOrtho(lrT.xAxis, lrT.yAxis, lrT.zAxis);
        const s32 liWheelsDown = lrCar.GetNumberOfWheelsOnTheGround();
        const bool lbAir = (liWheelsDown == 0);

        ++suDotBand[OrthoDotBand(lM.mfMaxDot)];
        ++suDetBand[OrthoDetBand(lM.mfDet)];
        if (lbAir) { ++suAirFrames; }
        if (lM.mfMaxDot > sfMaxDotEver) { sfMaxDotEver = lM.mfMaxDot; suMaxDotFrame = suFrame; }
        if (lM.mfDet    < sfMinDetEver) { sfMinDetEver = lM.mfDet;    suMinDetFrame = suFrame; }

        // A per-frame line while airborne (that is the state under test), else one every 30.
        if (suLines < 4000u && (lbAir || (suFrame % 30u) == 0u))
        {
            ++suLines;
            *CgsDev::Log::gpDebugPrint
                << "[ortho] f " << static_cast<s32>(suFrame)
                << " air " << (lbAir ? 1 : 0) << " wheelsDown " << liWheelsDown
                << " crash " << (lrCar.IsCrashing() ? 1 : 0)
                << " maxdot " << lM.mfMaxDot << " det " << lM.mfDet
                << " len " << lM.mfLenX << "," << lM.mfLenY << "," << lM.mfLenZ
                << " pos " << lrT.wAxis.x << "," << lrT.wAxis.y << "," << lrT.wAxis.z << "\n";
        }

        if ((suFrame % 300u) == 0u)
        {
            *CgsDev::Log::gpDebugPrint
                << "[ortho] CENSUS frames " << static_cast<s32>(suFrame)
                << " air " << static_cast<s32>(suAirFrames)
                << " | maxdot bands <1e-4 " << static_cast<s32>(suDotBand[0])
                << " <1e-3 " << static_cast<s32>(suDotBand[1])
                << " <1e-2 " << static_cast<s32>(suDotBand[2])
                << " <0.1 "  << static_cast<s32>(suDotBand[3])
                << " <0.3 "  << static_cast<s32>(suDotBand[4])
                << " <0.6 "  << static_cast<s32>(suDotBand[5])
                << " >=0.6 " << static_cast<s32>(suDotBand[6])
                << " | det bands >0.999 " << static_cast<s32>(suDetBand[0])
                << " >0.99 " << static_cast<s32>(suDetBand[1])
                << " >0.9 "  << static_cast<s32>(suDetBand[2])
                << " >0.5 "  << static_cast<s32>(suDetBand[3])
                << " >0.1 "  << static_cast<s32>(suDetBand[4])
                << " >0.01 " << static_cast<s32>(suDetBand[5])
                << " <=0.01 " << static_cast<s32>(suDetBand[6])
                << " | worst maxdot " << sfMaxDotEver << " @f " << static_cast<s32>(suMaxDotFrame)
                << " worst det " << sfMinDetEver << " @f " << static_cast<s32>(suMinDetFrame) << "\n";
        }
    }

    // The one shared leg of both bodies, byte-for-byte the same instruction group in each
    // (X360 0x82619AF8..0x82619B30 and 0x825EF914..0x825EF94C):
    //     v13 = <splatted KF_GRAVITY> * <splatted dt>
    //     v0  = vspltw(mLinearVelocity, 1)          ; lane 1 == .y
    //     v0  = v0 - v13
    //     v12 = vrlimi128(mLinearVelocity, v0, 4, 0); mask 4 == insert lane 1 only
    //     store v12 back over mLinearVelocity
    //     IntegrateTransform(dt)                    ; dt still live in v1
    // i.e. mLinearVelocity.y -= KF_GRAVITY * dt, then advance the pose. The console writes the
    // whole 16-byte register back with x/z/w carried through unchanged, which is what assigning
    // the single named lane expresses.
    inline void ApplyGravityAndIntegrate(ExternalPhysicsBody& lrBody, VecFloat lvfDeltaTime)
    {
        Vector3 lvLinearVelocity = lrBody.GetLinearVelocity();
        lvLinearVelocity.y -= KF_GRAVITY * lvfDeltaTime.x;
        lrBody.SetLinearVelocity(lvLinearVelocity);

        lrBody.IntegrateTransform(lvfDeltaTime);
    }
}

// -------------------------------------------------------------------------------------------
// VehicleManager::ReadUpdatedBodies   @0x82619A10   (DWARF :366)
//
// Body, read from the asm:
//   r19 = this + 44224 == &mUsedRaceCars  (`addis r19,r14,1 ; addi r19,r19,-0x5340`)
//   the two inlined BitArray<8u> scans (first-set-bit: field scan + isolate-lowest +
//   `word*64 + 63 - cntlzd`; next-set-bit: linear within the field then re-scan), with the
//   container's own bounds assert baked at CgsBitArray.h:203 -- expressed here through the
//   committed GetFirstNonZeroBit()/GetNextNonZeroBit() pair, whose bit math is that same
//   arithmetic, and with the assert hoisted to the call site (the committed BitArray header is
//   deliberately assert-free; the CrashingRaceCarInterface::SetFromVehicleOutputInterface
//   precedent in BrnVehicleOutputInterface.cpp does exactly this).
//   per set slot:  `mulli r11, r31, 0x1460 ; add r11,r11,r14 ; addi r11,r11,0x740`
//                  == &maRaceCarVehicles[slot]  (console stride 5216, base +1856)
//                  `lbz r10, 0x70(r11)` == mbFrozen -> skip when set
//                  the shared gravity+integrate leg above, on the +0x10 base sub-object
//   tail:          r3 = this + 44768, r4 = the (still unread) queue, v1 = dt
//                  -> PhysicalTrafficManager::ReadUpdatedBodies
//
// EVERY SEAT IS REACHED BY NAME. The console's 0x1460 stride is 5216 and the host's
// sizeof(RaceCarPhysics) is 5008 (BrnVehicleManager.h's KU_HOST_DRIFT_AFTER_RACECAR_ARRAY note),
// so an offset-based transcription of this function would have indexed into the wrong car from
// slot 1 onward while still compiling, linking and running.
// -------------------------------------------------------------------------------------------
void VehicleManager::ReadUpdatedBodies(
    const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody, 200>* lpUpdatedBodyQueue,
    VecFloat lvfTimeStep)
{
    for (s32 liRaceCar = mUsedRaceCars.GetFirstNonZeroBit();
         liRaceCar >= 0;
         liRaceCar = mUsedRaceCars.GetNextNonZeroBit(liRaceCar))
    {
        CGS_ASSERT(static_cast<u32>(liRaceCar) < 8u, "invalid index : < 8");   // CgsBitArray.h:203

        RaceCarPhysics& lrRaceCar = maRaceCarVehicles[liRaceCar];

        if (lrRaceCar.IsFrozen())
        {
            continue;
        }

        ApplyGravityAndIntegrate(lrRaceCar, lvfTimeStep);

        OrthoProbe(lrRaceCar, liRaceCar);   // [ortho] witness -- opt-in, read-only
    }

    // The queue is handed on exactly as received -- this function never looked inside it.
    mPhysicalTrafficManager.ReadUpdatedBodies(lpUpdatedBodyQueue, lvfTimeStep);
}

// -------------------------------------------------------------------------------------------
// PhysicalTrafficManager::ReadUpdatedBodies   @0x825EF608
//
// Two halves, and the FIRST is the only consumer of the queue anywhere in this pair:
//
//  1. A DEV DUPLICATE-DETECTION SWEEP. The O(n^2) double loop compares every event's leading
//     8-byte mID against every other's and fires "Recieved two update events for rigid body
//     0x%08X%08X" (the console's own spelling, typo included) at :1945 on a match. It reads
//     nothing else and changes nothing. `i == j` is skipped by the console's own `cmpw r22,r24 ;
//     beq` -- so the guard is index equality, not id equality.
//
//  2. The same gravity + IntegrateTransform leg as the race-car half, over the used-traffic
//     bitset (`addis r22,r20,2 ; addi r22,r22,-0x6798` == this + 104552 == mUsedTrafficVehicles,
//     capacity 20), through mpaTrafficVehicles[i].mpVehicleBody (`lwz 0x1C`), guarded by that
//     vehicle's mu8PhysicalType (`lbz 0x32`) being E_PHYSICAL_TRAFFIC_TYPE_FULL and by the body's
//     own mbFrozen (`lbz 0x70`).
//
// THE TYPE TEST IS EMITTED TWICE and both copies are kept. The console reads mu8PhysicalType,
// range-asserts it (BrnPhysicalTrafficVehicle.h:382) and branches on it; then RE-READS it,
// range-asserts it AGAIN and asserts "IsFullyPhysical()" (:391) before dereferencing
// mpVehicleBody. That is `if (!IsFullyPhysical()) continue;` followed by an inlined
// GetVehiclePhysics() which re-checks the same predicate -- an inlining artifact of the shipped
// source, reproduced rather than folded: folding it away would remove a shipped assert, and a
// shipped assert is behaviour.
// -------------------------------------------------------------------------------------------
void PhysicalTrafficManager::ReadUpdatedBodies(
    const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody, 200>* lpUpdatedBodies,
    VecFloat lvfTimeStep)
{
    CGS_ASSERT(lpUpdatedBodies != 0, "lpUpdatedBodies != NULL");     // :1932

    // ---- 1. the dev duplicate sweep (the queue's ONLY reader) --------------------------------
    for (s32 liEvent = 0; liEvent < lpUpdatedBodies->GetLength(); ++liEvent)
    {
        const u64 lu64Id = lpUpdatedBodies->GetEvent(liEvent).mID;

        for (s32 liOther = 0; liOther < lpUpdatedBodies->GetLength(); ++liOther)
        {
            if (liEvent == liOther)
            {
                continue;
            }

            CGS_ASSERT(lpUpdatedBodies->GetEvent(liOther).mID != lu64Id,
                       "Recieved two update events for rigid body 0x%08X%08X");   // :1945
        }
    }

    // ---- 2. gravity + integrate, per fully-physical traffic vehicle --------------------------
    for (s32 liVehicle = mUsedTrafficVehicles.GetFirstNonZeroBit();
         liVehicle >= 0;
         liVehicle = mUsedTrafficVehicles.GetNextNonZeroBit(liVehicle))
    {
        // (the console's assert text names the source-level constant ku8TotalMaxNumPhysical-
        //  Traffic; this tree spells that same 20 as KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC, and the
        //  message is kept verbatim as the repo convention requires)
        CGS_ASSERT(static_cast<u32>(liVehicle) < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "liVehicle < ku8TotalMaxNumPhysicalTraffic");                  // :741

        PhysicalTrafficVehicle& lrVehicle = mpaTrafficVehicles[liVehicle];

        // the first inlined copy of the predicate
        CGS_ASSERT(lrVehicle.mu8PhysicalType < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                   "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");                     // :382
        if (lrVehicle.mu8PhysicalType != PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL)
        {
            continue;
        }

        // the second copy, inlined from GetVehiclePhysics() -- kept as shipped
        CGS_ASSERT(lrVehicle.mu8PhysicalType < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                   "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");                     // :382
        CGS_ASSERT(lrVehicle.mu8PhysicalType == PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL,
                   "IsFullyPhysical()");                                          // :391

        SimpleVehiclePhysics* const lpBody = lrVehicle.mpVehicleBody;

        if (lpBody->IsFrozen())
        {
            continue;
        }

        ApplyGravityAndIntegrate(*lpBody, lvfTimeStep);
    }
}
}
}
