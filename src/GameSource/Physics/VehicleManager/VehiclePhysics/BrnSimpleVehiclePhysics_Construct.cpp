#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"             // rw::math::vpu::{IsValid, operator+/-, Mult, Dot}

// ============================================================================================
// BrnPhysics::Vehicle::SimpleVehiclePhysics::Construct @0x826203E8 -- SPLIT OUT of
// BrnSimpleVehiclePhysics.cpp on 2026-08-02 (physics wave 3). BUILD-MECHANICS SPLIT ONLY: the
// body below is byte-identical to the one that used to sit in that file and its declared home
// is unchanged.
//
// ⛔ THIS TU IS DELIBERATELY **NOT MOUNTED** in tools/build/build_game_exe.bat.
// It calls BrnPhysics::Vehicle::SimpleVehicleAttribs::Construct @0x825E6580, which has no body
// anywhere in the tree and cannot get one yet: the console function is a ~120-line lane-write
// initialiser over ~15 unresolved .rdata float constants, writing offsets +0x00..+0xE4 of a type
// this tree still models as a two-member minimal slice (mCOMOffset / mbIsValid). Bodying it needs
// the real VehicleAttribs layout pass; fabricating the constants is forbidden.
//
// Everything else in BrnSimpleVehiclePhysics.cpp -- GetGraphicsVehicleTransform (@0x825BF158, the
// function that produces the car's RENDER pose and the one
// VehicleOutputInterface::UpdateRaceCarState @0x825EC8F8 calls), SetGraphicsVehicleTransform,
// IsContactBelowWheelPlane, Destruct, Reset, SetAboveGroundTestResult, ClearCrashing -- IS mounted.
//
// TO RE-MERGE: body SimpleVehicleAttribs::Construct, then move this body back and delete the TU.
// ============================================================================================

namespace BrnPhysics
{
namespace Vehicle
{
    static const Vector3 KV_ZERO_CONSTRUCT = { 0.0f, 0.0f, 0.0f, 0.0f };

    // -------------------------------------------------------------------------------------------
    // Construct  @0x826203E8
    //   base Construct, Wheel::Clear each of the 4 wheels (the do/while walks +304 stride 224 until
    //   Wheel::Clear returns the sentinel), SimpleVehicleAttribs::Construct, zero
    //   mHandlingBodyOffset(+1680)/mHalfExtent(+1696)/the two AABBs(@+1392 stride 16, the
    //   `stvx128 v1,r11,r10` pair) and seed the flag words: *(+1430)=0x8000, *(+1428)=-1,
    //   *(+1424)=0.0, *(+1432)=0 -- these console scratch words land in the AABB/flag region; in the
    //   BY-NAME home they are reproduced as the deform/crash bool seeds + the wheel-plane reset, then
    //   Reset, then *(+112)=0 (the base engine-only gate cleared).
    // -------------------------------------------------------------------------------------------
    void SimpleVehiclePhysics::Construct()
    {
        // The X360 calls the DIRECT base's Construct on the base subobject (`this+16`):
        // `bl BrnPhysics__ExternalPhysicsBody__Construct`. ExternalPhysicsBody::Construct is
        // declared (ExternalPhysicsBody.h) with its body in that TU; call it BY NAME.
        ExternalPhysicsBody::Construct();
        for (int liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
            maWheels[liWheel].Clear();
        mSimpleAttribs.Construct();

        mHandlingBodyOffset = KV_ZERO_CONSTRUCT;                 // +1680
        mHalfExtent         = KV_ZERO_CONSTRUCT;                 // +1696
        mDeformableAABB.mMin = KV_ZERO_CONSTRUCT;                // +1392 region (the stvx128 v1 zero pair)
        mDeformableAABB.mMax = KV_ZERO_CONSTRUCT;
        mOriginalAABB.mMin   = KV_ZERO_CONSTRUCT;
        mOriginalAABB.mMax   = KV_ZERO_CONSTRUCT;

        // FLAG: the X360 seeds raw scratch words *(+1430)=0x8000 / *(+1428)=-1 / *(+1424)=0.0 /
        // *(+1432)=0 that sit in the deform/crash-flag/wheel-plane region. In the BY-NAME home the
        // faithful intent is the post-construct reset state, applied by Reset() below.
        Reset();
        // *(this+112)=0 -- the base sleep/engine-only-update gate (mbFrozen region), cleared.
        SetFrozen(false);
    }
}
}
