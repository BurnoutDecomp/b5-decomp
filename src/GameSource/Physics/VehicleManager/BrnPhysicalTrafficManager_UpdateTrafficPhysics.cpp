// ============================================================================
// BrnPhysicalTrafficManager_UpdateTrafficPhysics.cpp
//
//   BrnPhysics::Vehicle::PhysicalTrafficManager::UpdateTrafficPhysics @0x82644418 (259)
//
// The per-frame tick of every FULLY physical traffic body: install the driver's controls into
// the body, then run the full-physics update. Sole caller VehicleManager::UpdateVehiclePhysics.
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


namespace BrnPhysics
{
namespace Vehicle
{
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


}
}
}
