// ============================================================================
// BrnPhysicalTrafficManager_UpdateTrafficPhysics.cpp
//
//   BrnPhysics::Vehicle::PhysicalTrafficManager::UpdateTrafficPhysics @0x82644418 (259)
//
// The per-frame tick of every FULLY physical traffic body: install the driver's controls into
// the body, then run the full-physics update. Sole caller VehicleManager::UpdateVehiclePhysics
// @0x82645E44. Its link stub at BrnVehicleManagerLinkStubs.cpp:284 must be DELETED with this.
//
// The address was an .ida-exports HOLE (absent from progress/identity.json); the body was dumped
// headless from a COPY of the ARTIST .i64. DWARF BrnPhysicalTrafficManager.h:152
//   void UpdateTrafficPhysics(float32_t, float32_t, const Matrix44Affine*, bool, bool)
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnVehicleDriver.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdlib>   // getenv (BRN_TRAFFIC_DIAG)
#include <cmath>     // sqrtf ([T5-ram] velocity magnitude)

namespace BrnPhysics
{
namespace Vehicle
{
// =================================================================================================
// [T5-ram] DIAG STATE. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
//
// Defined here (the per-frame traffic tick is the natural print site); the arming sites live in
// BrnPhysicalTrafficManager_CrashResponse.cpp, the player pose is published by
// BrnVehicleManager_UpdateVehiclePhysics.cpp, the deformation counters are bumped by the
// DeformationManager contact bridge / DeformableObject impulse TUs, and the module-side pose is
// printed by TrafficEntityModule::HandleExternalResponses -- all through `extern` declarations, so
// no header this cluster does not own is touched.
// =================================================================================================
s32 gT5RamFramesLeft  = 0;     // > 0 while the post-hit measurement window is open
s32 gT5RamTrafficSlot = -1;    // the PHYSICAL traffic slot that was hit
s32 gT5RamGlobalIndex = -1;    // ...and its GLOBAL traffic vehicle index (for the module side)
f32 gT5PlayerPos[3]   = { 0.0f, 0.0f, 0.0f };
f32 gT5PlayerVel[3]   = { 0.0f, 0.0f, 0.0f };
u32 gT5Q8Routed       = 0;     // queue-[8] contacts handed to DeformationManager::ReadPotentialContact
u32 gT5Q8Accepted     = 0;     // ...of which DeformationSensor::ValidateAndAddContact kept one
u32 gT5CarCarCalls    = 0;     // DeformableObject::ApplyCarCarImpulse entries
u32 gT5CarCarApplied  = 0;     // ...that returned true (an impulse actually went into both bodies)
f32 gT5LastImpulseMag = 0.0f;  // the last car-car impulse LENGTH handed to ApplySensorImpulse
f32 gT5LastClosing    = 0.0f;  // ...and the closing speed it was solved from
f32 gT5MaxImpulseMag  = 0.0f;  // the LARGEST car-car impulse length since the window was (re)armed
f32 gT5SumImpulseMag  = 0.0f;  // ...and their running sum -- the momentum actually delivered
f32 gT5MaxClosing     = 0.0f;  // the largest closing speed a car-car solve ran on since (re)arming
f32 gT5ArmedPlayerSpeed = 0.0f; // |player velocity| at the instant the window was (re)armed
s32 gT5ApplyOwner     = -1;    // [T5-sens] the handling-body OWNER byte of the DeformableObject whose
s32 gT5ApplyGlobal    = -1;    //   ApplySensorImpulse is dispatching into a sensor right now, and its
                               //   global entity index -- so the sensor-side [impulse] probe can tag
                               //   the traffic car's applies apart from the player's.
s32 gT5WindowFrame    = 0;     // frames since the window was (re)armed -- reset by ArmT5RamWindow, so
                               // the every-frame impact stretch is per WINDOW (the old global
                               // counter left a re-armed window sampling every 10th frame: the
                               // 109 mph CHECK in run tw_ram1 has no impact-frame rows).

namespace
{
    // DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    bool TrafficDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
        return sbEnabled;
    }

    // [T5-ram] one line per print, in two halves (pre-tick / post-tick) so "the impulse landed and
    // TrafficPhysics::Update then wiped it" is distinguishable from "no impulse ever landed".
    void PrintT5Line(const char* lpcWhen, s32 liFrame, s32 liSlot,
                     const PhysicalTrafficVehicle& lrVehicle, const VehicleDriver* lpDriver)
    {
        const SimpleVehiclePhysics* const lpBody = lrVehicle.mpVehicleBody;
        if (lpBody == 0 || CgsDev::Log::gpDebugPrint == 0)
        {
            return;
        }
        const Vector3 lvVel = lpBody->GetLinearVelocity();
        const Vector3 lvPos = lpBody->GetTransform().wAxis;
        const f32 lfSpeed = sqrtf(lvVel.x * lvVel.x + lvVel.y * lvVel.y + lvVel.z * lvVel.z);
        const f32 lfPlayerSpeed = sqrtf(gT5PlayerVel[0] * gT5PlayerVel[0]
                                        + gT5PlayerVel[1] * gT5PlayerVel[1]
                                        + gT5PlayerVel[2] * gT5PlayerVel[2]);

        *CgsDev::Log::gpDebugPrint
            << "[T5-ram] " << lpcWhen << " f=" << liFrame << " slot=" << liSlot
            << " state=" << static_cast<s32>(lrVehicle.mePhysicalTrafficState)
            << " ptype=" << static_cast<s32>(lrVehicle.mu8PhysicalType)
            << " frozen=" << static_cast<s32>(lpBody->IsFrozen() ? 1 : 0)
            << " mass=" << lpBody->GetMass().x
            << " tpos=(" << lvPos.x << "," << lvPos.y << "," << lvPos.z << ")"
            << " tvel=(" << lvVel.x << "," << lvVel.y << "," << lvVel.z << ")"
            << " |tv|=" << lfSpeed;

        // The three numbers a "does it crash hard" claim needs beside the speed: is the body in
        // its crashing state at all, how fast is it spinning, and has it left the upright.
        {
            const Vector3 lvW = lpBody->GetAngularVelocity();
            *CgsDev::Log::gpDebugPrint
                << " crashing=" << static_cast<s32>(lpBody->IsCrashing() ? 1 : 0)
                << " |w|=" << sqrtf(lvW.x * lvW.x + lvW.y * lvW.y + lvW.z * lvW.z)
                << " upY=" << lpBody->GetTransform().yAxis.y;
        }

        // The traffic body's DEFORMED AABB (SimpleVehiclePhysics::mDeformableAABB @+0x6D0/+0x6E0,
        // the pair DeformableObject::UpdateDeformedBBox writes back for every model, traffic
        // included) and its drift from the first sample of this window -- the traffic-side
        // deformation reading. NOTE the console's drive-time-limit compare in UpdateDeformedBBox
        // (0x825E0DB4..DC4) runs for RACE CAR owners only; a traffic model has no limit verdict.
        {
            static f32 s_afFirstBox[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            static s32 s_iFirstBoxSlot = -1;
            static s32 s_iFirstBoxArm  = -1;
            const CgsGeometric::AxisAlignedBox& lrBox = lpBody->GetDeformableAABB();
            const f32 lafNow[6] = { lrBox.mMin.x, lrBox.mMin.y, lrBox.mMin.z,
                                    lrBox.mMax.x, lrBox.mMax.y, lrBox.mMax.z };
            if (s_iFirstBoxSlot != liSlot || s_iFirstBoxArm != gT5RamGlobalIndex)
            {
                s_iFirstBoxSlot = liSlot;
                s_iFirstBoxArm  = gT5RamGlobalIndex;
                for (s32 li = 0; li < 6; ++li) s_afFirstBox[li] = lafNow[li];
            }
            *CgsDev::Log::gpDebugPrint
                << " dmin=(" << lafNow[0] << "," << lafNow[1] << "," << lafNow[2] << ")"
                << " dmax=(" << lafNow[3] << "," << lafNow[4] << "," << lafNow[5] << ")"
                << " ddelta=(" << (lafNow[0] - s_afFirstBox[0]) << "," << (lafNow[1] - s_afFirstBox[1])
                << "," << (lafNow[2] - s_afFirstBox[2]) << "," << (lafNow[3] - s_afFirstBox[3])
                << "," << (lafNow[4] - s_afFirstBox[4]) << "," << (lafNow[5] - s_afFirstBox[5]) << ")";
        }

        if (lpDriver != 0)
        {
            const BrnPlayerDriverControls* const lpControls = lpDriver->GetControls();
            *CgsDev::Log::gpDebugPrint
                << " gas=" << lpControls->mfGas
                << " brake=" << lpControls->mfBrake
                << " hb=" << lpControls->mfHandBrake
                << " steer=" << lpControls->mfSteering;
        }

        *CgsDev::Log::gpDebugPrint
            << " ppos=(" << gT5PlayerPos[0] << "," << gT5PlayerPos[1] << "," << gT5PlayerPos[2] << ")"
            << " pvel=(" << gT5PlayerVel[0] << "," << gT5PlayerVel[1] << "," << gT5PlayerVel[2] << ")"
            << " |pv|=" << lfPlayerSpeed
            << " q8routed=" << static_cast<s32>(gT5Q8Routed)
            << " q8acc=" << static_cast<s32>(gT5Q8Accepted)
            << " ccCalls=" << static_cast<s32>(gT5CarCarCalls)
            << " ccApplied=" << static_cast<s32>(gT5CarCarApplied)
            << " lastMag=" << gT5LastImpulseMag
            << " lastClose=" << gT5LastClosing
            << " maxMag=" << gT5MaxImpulseMag
            << " sumMag=" << gT5SumImpulseMag
            << " maxClose=" << gT5MaxClosing
            << " armedPv=" << gT5ArmedPlayerSpeed
            << "\n";
    }
}

// -------------------------------------------------------------------------------------------
// @0x82644418. Structure, read off the asm:
//
//   0x8264442C  r16 = this + 104552  == &mUsedTrafficVehicles   -- the ONLY bitset it walks
//   the inlined GetFirstNonZeroBit / GetNextNonZeroBit pair (word scan, isolate-lowest,
//   `word*64 + 63 - cntlzd`, then the linear-within-field re-scan) with the container's own
//   CgsBitArray.h:203 "invalid index" assert baked in -- expressed here through the committed
//   GetFirstNonZeroBit()/GetNextNonZeroBit() and the assert hoisted to the call site, exactly
//   as BrnVehicleManager_ReadUpdatedBodies.cpp does for the same bitset.
//   per set slot:
//     0x82644564  `slwi r11,r28,6 ; add r30,r11,r10` == &mpaTrafficVehicles[i] (stride 64)
//     0x8264456C  `lbz r31,0x32(r30)` == mu8PhysicalType, range-asserted (h:382)
//                 SKIP unless FULL; then the type is RE-READ, range-asserted AGAIN and
//                 IsFullyPhysical()-asserted (h:391) -- an inlined GetVehiclePhysics(), the
//                 same doubled predicate ReadUpdatedBodies carries. Kept, not folded.
//     0x826445DC  `lwz r29,0x1C(r30)` == mpVehicleBody
//     0x826445E0  the SECOND bound assert, h:752, before the driver pool is indexed
//     0x82644604  `mulli r10,r28,0xE0 ; lwzx r11,this,0x194B0` == &mpaTrafficDrivers[i]
//     0x82644618  VehicleDriver::UpdateVehicle(driver, body)
//     0x8264463C  PhysicalTrafficVehicle::Update(vehicle, ...)
//
// THE CONTROLS ARGUMENT IS THE DRIVER ITSELF: r7 into PhysicalTrafficVehicle::Update is
// `mr r7, r31` == the raw &mpaTrafficDrivers[i], which IS &driver.mControls (BrnVehicleDriver.h
// pins mControls at +0). Spelled by name here through VehicleDriver::GetControls().
//
// THE LAST ARGUMENT IS A HARD ZERO: `li r10, 0` @0x8264461C -- lbUseSixaxis is false for traffic
// every frame, not a forwarded parameter. lbUnknownFalse (r8) forwards into
// lbDoForceAdditiveAftertouch (r9), and lbImpactTime into r8.
//
// X360 STRIDES: the console reaches both pools by `slwi 6` (64) and `mulli 0xE0` (224). Both are
// reached BY NAME here; sizeof(PhysicalTrafficVehicle) is NOT 64 on the host (the pointer member
// widens), so an offset transcription would index the wrong vehicle from slot 1 onward.
// -------------------------------------------------------------------------------------------
void PhysicalTrafficManager::UpdateTrafficPhysics(f32 lfSimTimeStep, f32 lfGameTimeStep,
                                                  const Matrix44Affine* lpCameraMatrix,
                                                  bool lbImpactTime, bool lbUnknownFalse)
{
    // ---- [T5-ram] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. -------------------------
    // The measurement window the finisher round asked for: every 10th frame for ~3 s after the
    // first race-car-vs-traffic outcome, print the hit car's whole physical state twice -- once
    // BEFORE the driver+physics tick and once AFTER it -- so a wiped velocity and an absent
    // impulse are two different readings rather than one ambiguous zero.
    s32 liT5Frame = 0;
    bool lbT5Print = false;
    if (gT5RamFramesLeft > 0)
    {
        --gT5RamFramesLeft;
        liT5Frame = ++gT5WindowFrame;
        // Every frame for the first half second (the impact lives there), then every 10th.
        lbT5Print = (liT5Frame <= 30 || (liT5Frame % 10) == 1) && TrafficDiagEnabled() && CgsDev::Log::gpDebugPrint != 0;
    }
    if (lbT5Print && gT5RamTrafficSlot >= 0
        && static_cast<u32>(gT5RamTrafficSlot) < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC
        && mUsedTrafficVehicles.IsBitSet(static_cast<u32>(gT5RamTrafficSlot)))
    {
        PrintT5Line("pre ", liT5Frame, gT5RamTrafficSlot,
                    mpaTrafficVehicles[gT5RamTrafficSlot], &mpaTrafficDrivers[gT5RamTrafficSlot]);
    }

    for (s32 liVehicle = mUsedTrafficVehicles.GetFirstNonZeroBit();
         liVehicle != TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX;
         liVehicle = mUsedTrafficVehicles.GetNextNonZeroBit(liVehicle))
    {
        CGS_ASSERT(static_cast<u32>(liVehicle) < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "liVehicle < ku8TotalMaxNumPhysicalTraffic");                       // h:741

        PhysicalTrafficVehicle& lrVehicle = mpaTrafficVehicles[liVehicle];

        CGS_ASSERT(lrVehicle.mu8PhysicalType < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                   "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");                          // h:382
        if (lrVehicle.mu8PhysicalType != PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL)
        {
            continue;
        }

        // the second copy of the predicate, inlined from GetVehiclePhysics() -- kept as shipped
        CGS_ASSERT(lrVehicle.mu8PhysicalType < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                   "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");                          // h:382
        CGS_ASSERT(lrVehicle.mu8PhysicalType == PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL,
                   "IsFullyPhysical()");                                               // h:391

        SimpleVehiclePhysics* const lpBody = lrVehicle.mpVehicleBody;

        CGS_ASSERT(static_cast<u32>(liVehicle) < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "liVehicle < ku8TotalMaxNumPhysicalTraffic");                       // h:752

        VehicleDriver& lrDriver = mpaTrafficDrivers[liVehicle];

        // The driver installs its latest controls into the body (BrnVehicleDriver.cpp @0x825D7290).
        lrDriver.UpdateVehicle(reinterpret_cast<VehiclePhysics*>(lpBody));

        lrVehicle.Update(lfSimTimeStep, lfGameTimeStep, lpCameraMatrix,
                         lrDriver.GetControls(), lbImpactTime, lbUnknownFalse,
                         false /* lbUseSixaxis -- `li r10, 0` @0x8264461C */);
    }

    // ---- [T5-ram] the POST-tick half of the same frame. DELETE-WHEN-STABLE. ------------------
    if (lbT5Print && gT5RamTrafficSlot >= 0
        && static_cast<u32>(gT5RamTrafficSlot) < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC
        && mUsedTrafficVehicles.IsBitSet(static_cast<u32>(gT5RamTrafficSlot)))
    {
        PrintT5Line("post", liT5Frame, gT5RamTrafficSlot,
                    mpaTrafficVehicles[gT5RamTrafficSlot], &mpaTrafficDrivers[gT5RamTrafficSlot]);
    }
}
}
}
