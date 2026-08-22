// ============================================================================
// BrnPhysicalTrafficManager_UpdateTrafficPhysics.cpp
//
//   BrnPhysics::Vehicle::PhysicalTrafficManager::UpdateTrafficPhysics @0x82644418 (259)
//
// The per-frame tick of every FULLY physical traffic body: install the driver's controls into
// the body, then run the full-physics update. Sole caller VehicleManager::UpdateVehiclePhysics
// @0x82645E44. Its link stub at BrnVehicleManagerLinkStubs.cpp:284 must be DELETED with this.
//
// The address was an .ida-exports HOLE (and is absent from progress/identity.json); the body was
// dumped headless from a COPY of the ARTIST .i64 during the wave-T3 scout
// (scratchpad .../wave3/scout/holes/0x82644418.txt: pseudocode + full asm + xrefs).
// DWARF BrnPhysicalTrafficManager.h:152
//   void UpdateTrafficPhysics(float32_t, float32_t, const Matrix44Affine*, bool, bool)
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdlib>   // getenv (BRN_TRAFFIC_DIAG)
#include <cmath>     // sqrtf ([T3-tick] velocity magnitude)

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // [T3-tick] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    bool TrafficDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
        return sbEnabled;
    }
    bool s_bTickReported = false;
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
// ⚠️ THE CONTROLS ARGUMENT IS THE DRIVER ITSELF. r7 into PhysicalTrafficVehicle::Update is
// `mr r7, r31` -- the raw &mpaTrafficDrivers[i]. That address IS &driver.mControls
// (BrnVehicleDriver.h pins mControls at +0), so the console is passing the driver's control
// block; spelled BY NAME here through VehicleDriver::GetControls().
//
// ⚠️ THE LAST ARGUMENT IS A HARD ZERO. `li r10, 0` at 0x8264461C -- lbUseSixaxis is false for
// traffic on every frame, not a forwarded parameter. lbUnknownFalse (this function's own r8)
// is forwarded into lbDoForceAdditiveAftertouch (r9), and lbImpactTime into r8.
//
// ⚠️ NOTE ON THE X360 STRIDES: the console reaches both pools by `slwi 6` (64) and `mulli 0xE0`
// (224). Both are reached BY NAME here; sizeof(VehicleDriver) is 224 on the host too, but
// sizeof(PhysicalTrafficVehicle) is NOT 64 on the host (the pointer member widens), so an
// offset transcription would have indexed into the wrong vehicle from slot 1 onward.
// -------------------------------------------------------------------------------------------
void PhysicalTrafficManager::UpdateTrafficPhysics(f32 lfSimTimeStep, f32 lfGameTimeStep,
                                                  const Matrix44Affine* lpCameraMatrix,
                                                  bool lbImpactTime, bool lbUnknownFalse)
{
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

        // [T3-tick] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
        if (!s_bTickReported && TrafficDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
        {
            s_bTickReported = true;
            const Vector3 lvVelocity = lpBody->GetLinearVelocity();
            *CgsDev::Log::gpDebugPrint
                << "[T3-tick] UpdateTrafficPhysics first live slot=" << liVehicle
                << " simDt=" << lfSimTimeStep
                << " |v|=" << sqrtf(lvVelocity.x * lvVelocity.x
                                    + lvVelocity.y * lvVelocity.y
                                    + lvVelocity.z * lvVelocity.z)
                << "\n";
        }

        lrVehicle.Update(lfSimTimeStep, lfGameTimeStep, lpCameraMatrix,
                         lrDriver.GetControls(), lbImpactTime, lbUnknownFalse,
                         false /* lbUseSixaxis -- `li r10, 0` @0x8264461C */);
    }
}
}
}
