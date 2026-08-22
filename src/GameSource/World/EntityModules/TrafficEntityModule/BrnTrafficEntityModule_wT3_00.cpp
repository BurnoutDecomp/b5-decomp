// ============================================================================
// BrnTrafficEntityModule_wT3_00.cpp -- wave T3 round 1 (PHYSICAL TRAFFIC) keystone leaves.
// The three tiny shared accessors every other wave-T3 cluster calls.
//
//   TrafficEntityModule::GetVehicle(u32) const                 DWARF :1230 (ICF twin of :1227)
//   TrafficEntityModule::GetTrafficPhysicsInfoForVehicl  @0x82714500 (153)
//   TrafficEntityModule::GetCarAssetAttribKey            @0x8273EFC8 (59)
//   TrafficEntityModule::CalculateInitialPhysicalState   @0x8271DD30 (96)
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/vector3_operation.h"          // Mult(Vector3, f32)
#include "rw/math/vpu/matrix44affine_operation.h"   // Mult(Matrix44Affine, Matrix44Affine)

namespace BrnTraffic
{
    // DWARF BrnTrafficEntityModule.h:1230. The console ICF-folds it onto the non-const
    // GetVehicle @leak :1590 (identical body); reproduced as a delegation.
    const Vehicle* TrafficEntityModule::GetVehicle(u32 luIndex) const
    {
        return const_cast<TrafficEntityModule*>(this)->GetVehicle(luIndex);
    }

    // @0x82714500 (153). DWARF :1242 `TrafficPhysicsInfo* GetTrafficPhysicsInfoForVehicle(u32)`
    // -- the ledger/X360 symbol truncates the name, which is the spelling kept here.
    // Six baked asserts, at the console's own file/lines (BrnTrafficEntityModule.h
    // :2500 / :2501 / :2504 / :2505 / :2506 / :2510, plus the inlined GetVehicle :2459 twice
    // and the BitArray.h:203 index message).
    // ⚠️ GetPhysicalPartsIndex is ZERO-extended in this tree (BrnTrafficVehicle.h:291, matching
    // the console's `lbz` with no extsb) while THIS caller sign-extends it (0x827145C4 extsb).
    // A -1 index therefore reads 255 here and trips the < 25 assert instead of the >= 0 one.
    // Same set of asserts fires either way; do not "fix" it by comparing against -1.
    TrafficPhysicsInfo* TrafficEntityModule::GetTrafficPhysicsInfoForVehicl(u32 luVehicle)
    {
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luVehicle < KU_MAX_TOTAL_TRAFFIC");
        CGS_ASSERT(GetVehicle(luVehicle)->IsPhysical(), "GetVehicle( luVehicle )->IsPhysical()");

        const s32 liPartsIndex = GetVehicle(luVehicle)->GetPhysicalPartsIndex();
        CGS_ASSERT(liPartsIndex >= 0, "liPartsIndex >= 0");
        CGS_ASSERT(liPartsIndex < static_cast<s32>(KU_MAX_PHYSICAL_TRAFFIC_VEHICLES),
                   "liPartsIndex < (int32_t)KU_MAX_PHYSICAL_TRAFFIC_VEHICLES");
        CGS_ASSERT(maTrafficPhysicsInfoListBits.IsBitSet(static_cast<u32>(liPartsIndex)),
                   "maTrafficPhysicsInfoListBits.IsBitSet( liPartsIndex )");

        TrafficPhysicsInfo* lpPhysicsInfo = &maTrafficPhysicsInfoList[liPartsIndex];

        // 0x8271472C `lhz 0x100A` + `extsh` + `cmpw`: the record's owning-vehicle halfword,
        // SIGN-extended, against luVehicle. The assert text names the DWARF's miVehicleIndex.
        CGS_ASSERT(static_cast<s32>(static_cast<s16>(lpPhysicsInfo->muOwningVehicleIndex))
                       == static_cast<s32>(luVehicle),
                   "lpPhysicsInfo->miVehicleIndex == (int32_t)luVehicle");
        return lpPhysicsInfo;
    }

    // The const twin (DWARF :1245). One console body serves both.
    const TrafficPhysicsInfo* TrafficEntityModule::GetTrafficPhysicsInfoForVehicl(u32 luVehicle) const
    {
        return const_cast<TrafficEntityModule*>(this)->GetTrafficPhysicsInfoForVehicl(luVehicle);
    }

    // @0x8273EFC8 (59). DWARF :1812 `const Attribute::Key GetCarAssetAttribKey(uint32_t) const`.
    // Asserts (BrnTrafficEntityModule.cpp:17130/:17131, the inlined GetVehicle .h:2467 and
    // Vehicle::IsAlive BrnTrafficVehicle.h:786), then hands back the vehicle TYPE's attrib key.
    VehicleTypeRuntime::AttribKey TrafficEntityModule::GetCarAssetAttribKey(u32 luVehicle) const
    {
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luVehicle < KU_MAX_TOTAL_TRAFFIC");
        CGS_ASSERT(mpVehicleList != 0, "mpVehicleList != NULL");

        const Vehicle* lpVehicle = GetVehicle(luVehicle);
        CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()");

        return GetVehicleTypeRuntime(lpVehicle->GetVehicleType())->GetAttribKey();
    }

    // @0x8271DD30 (96). DWARF :1578. The Feb-2007 leak spells this body verbatim
    // (BrnTrafficEntityModule.cpp:1410) and the asm agrees statement for statement.
    void TrafficEntityModule::CalculateInitialPhysicalState(
        const Vehicle* lpInVehicle,
        Matrix44Affine lVehicleTransform,
        Vector3& lOutInitialVelocity,
        Vector3& lOutAngularVelocity,
        u8* lpuOutAttribsId,
        Matrix44Affine& lOutTransform) const
    {
        // 0x8271DD60 loads the transform row at +0x20 (zAxis == At()) and 0x8271DD7C scales it
        // by GetSpeed's broadcast lane.
        lOutInitialVelocity = rw::math::vpu::Mult(lVehicleTransform.At(),
                                                  lpInVehicle->GetSpeed().x);

        lOutAngularVelocity.SetZero();

        *lpuOutAttribsId = 0;

        CGS_ASSERT(lpInVehicle->IsAlive(), "IsAlive()");
        const VehicleTypeRuntime* lpVehicleTypeRuntime =
            GetVehicleTypeRuntime(lpInVehicle->GetVehicleType());

        // lOutTransform = Matrix44AffineFromTranslation( mBBoxOffset ) * lVehicleTransform.
        // The console emits the GENERIC affine product with the translation matrix's constant
        // rows unfolded (0x8271DE20..0x8271DE98), which is why the first three result rows are
        // the source rows and only the translation row picks the offset up in vehicle axes.
        // ⚠️ SIGN: the read-back (HandleExternalResponses) applies Negate(mBBoxOffset). Adding
        // here and subtracting there is the round trip; flipping either makes every promoted
        // car jump by the bbox offset on its first physical frame.
        // FLAG: rw::math::vpu has no Matrix44AffineFromTranslation on this tree; the identity
        // seed plus the offset row IS that helper (the console's three constant rows are
        // vspltisw/vcfsx-built at 0x8271DDE8..0x8271DE28, its fourth is the raw mBBoxOffset load).
        Matrix44Affine lBBoxTranslate;
        lBBoxTranslate.SetIdentity();
        lBBoxTranslate.wAxis = lpVehicleTypeRuntime->GetBBoxOffset();

        lOutTransform = rw::math::vpu::Mult(lBBoxTranslate, lVehicleTransform);
    }
}
