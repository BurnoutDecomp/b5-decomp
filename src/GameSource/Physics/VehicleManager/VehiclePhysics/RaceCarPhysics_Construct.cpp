#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"

// ============================================================================================
// BrnPhysics::Vehicle::RaceCarPhysics::Construct -- SPLIT OUT of RaceCarPhysics.cpp on
// BUILD-MECHANICS SPLIT ONLY: the body below is byte-identical to the one that was
// written into that file and its declared home is unchanged. Same precedent, same reason, as
// BrnSimpleVehiclePhysics_Construct.cpp.
//
// WHY THE SPLIT. VehicleManager::Construct's eight-car loop calls this function, so mounting
// BrnVehicleManager.cpp requires it to be linkable. RaceCarPhysics.cpp as a whole CANNOT be
// mounted -- its Update path reads flt_820037C8 (the AI crash-timer re-seed) and unk_82FB8880
// (KVF_AI_CRASH_SLOWMO_FACTOR), neither of which has been read out of the image yet, so that TU
// still carries unresolved constants. This body reads none of them: its only callee is
// VehiclePhysics::Construct, which is bodied in the already-mounted VehiclePhysics.cpp.
//
// DO NOT re-merge this into RaceCarPhysics.cpp while that file is unmountable. TO RE-MERGE:
// read the two constants, mount RaceCarPhysics.cpp, then move this body back and delete the TU.
// ============================================================================================

namespace BrnPhysics
{
namespace Vehicle
{
    // ---------------------------------------------------------------------------------------
    // Construct  -- INLINED at 0x8263BF10..0x8263BF38 inside the eight-car loop of
    //               VehicleManager::Construct @0x8263B7C8; called OUT OF LINE by the PS3 DecFIGS
    //               build as RaceCarPhysics::Construct @0x6EB3D4, whose whole body is these same
    //               seven statements at Δ = −16.
    //
    // The X360 loop body, verbatim (r29 == record + 0x140D, a biased cursor; r20 == −0x1D;
    // f31 == flt_82001CC0 == 0.0f; v127 == vspltisw128 0; r11 == record + 0x1070):
    //     0x8263BF10  bl        VehiclePhysics::Construct     (r3 == r29 - 0x140D == record + 0)
    //     0x8263BF14  addi      r11, r29, -0x39D              -> record + 0x1070
    //     0x8263BF18  stfs      f31, -0xD(r29)                -> record + 0x1400
    //     0x8263BF1C  stb       r30, -1(r29)                  -> record + 0x140C
    //     0x8263BF20  stfs      f31, -5(r29)                  -> record + 0x1408
    //     0x8263BF24  stb       r30, 0(r29)                   -> record + 0x140D
    //     0x8263BF28  stvx128   v127, r29, r20                -> record + 0x13F0
    //     0x8263BF30  lvx128    v0, r0, r11
    //     0x8263BF34  vrlimi128 v0, v127, 2, 0                -> lane .z only (mask 2 == z)
    //     0x8263BF38  stvx128   v0, r0, r11
    // Every one of those six offsets is a NAMED member of this class's own block (the map in
    // RaceCarPhysics.h), and 0x1070 is a named VehiclePhysics register. Nothing here is a poke.
    //
    // The order below is the ISSUE order, not the declaration order -- the X360 writes
    // mfTimeSinceTookDownPlayer, mbPlayerCarInShowtime, mfBeachedTime, mbUsingAftertouch,
    // mPropCollisionImpulseSum, then the lane insert. Kept as issued because the pairing
    // (0x1400/0x140C then 0x1408/0x140D) is what the scheduler produced from the source order and
    // reordering it would lose the only evidence of what that source order was.
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::Construct()
    {
        VehiclePhysics::Construct();

        mfTimeSinceTookDownPlayer = 0.0f;   // +0x1400  (stfs f31, -0xD(r29))
        mbPlayerCarInShowtime     = false;  // +0x140C  (stb  r30, -1(r29))
        mfBeachedTime             = 0.0f;   // +0x1408  (stfs f31, -5(r29))
        mbUsingAftertouch         = false;  // +0x140D  (stb  r30, 0(r29))

        mPropCollisionImpulseSum.SetZero(); // +0x13F0  (stvx128 v127, r29, r20)

        // +0x1070 LANE Z ONLY -- a read-modify-write, exactly as VehiclePhysics::Construct already
        // does to the same lane a few instructions earlier. Redundant in both builds; reproduced.
        mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.z = 0.0f;
    }
}
}
