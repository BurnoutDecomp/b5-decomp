// ============================================================================
// BrnTraffic::VehicleTypeRuntime::Prepare @0x82761B10 -- the only function in this file.
// DWARF declaration shape at BrnTrafficVehicleTypeRuntime.h:66, member names :94..:112;
// Feb-2007 has no VehicleTypeRuntime::Prepare, so the ARTIST asm arbitrates alone.
//
// The eight stores the console makes, and the state of each here:
//   std  r4, 0x40(r16)                     mAttribKey                    REAL
//   lvx v0,r28,1680 ; stvx v0,r0,r16       mBBoxOffset      (this+0x00)  REAL
//   lvx v0,r28,64   ; stvx v0,r16,16       mBBoxHalfSize    (this+0x10)  REAL
//   vrlimi v12,v0,1,3   -> this+0x20 .w    ..._FwdAxle                   REAL
//   vrlimi v0, v13,2,0  -> this+0x20 .z    ..._BackAxle                  REAL
//   vrlimi v12,v127,4,0 -> this+0x30 .y    mMass_WheelRadius_Z_W.y       REAL (wheel radius)
//   vrlimi v12,v13, 8,0 -> this+0x30 .x    mMass_WheelRadius_Z_W.x       REAL
//   stb 0x5C(r16) + 0x48+i                 miNumPaintColours / maiPaint  REAL
//   vrlimi v13,v0,8,2 @0x82761F24          .x mCabPivot     (tag 29)     GATED (leg 3)
//   vrlimi v13,v0,4,1 @0x82761F88          .y mTrailerPivot (tag 28)     GATED (leg 3)
//
// Spec offsets resolve to NAMED members through the static_asserts already in
// BrnStreamedDeformationSpec.h: +64 mHandlingBodyDimensions, +80/+96 +48*i the wheel
// mPosition/mScale pair, +1552 mCarModelSpaceToHandlingBodySpaceTransform, +1632/+1648
// mCurrentCOMOffset/mMeshOffset, +1664/+1680 mRigidBodyOffset/mCollisionOffset, +36
// mGenericTags. The 1552/1632/1648 pins fix the tail run's phase, so +1680 has no slack.
//
// VMX lane merges: `vrlimi128 vD, vS, mask, sh` rotates vS and inserts the mask-selected
// words into vD, mask 8/4/2/1 == words 0/1/2/3. The axle pair uses masks 1 (.w FwdAxle) and
// 2 (.z BackAxle) with rotates 3 and 0, which put the SAME source lane -- word 2, the wheel's
// mPosition.z -- into both. Leg 3's masks 8/4 (words 0/1) are disjoint from these, so gating
// it cannot disturb the axle lanes.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"
#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"
#include "GameSource/AttribSys/Generated/classes/burnoutcargraphicsasset.h"
#include "GameSource/AttribSys/Generated/classes/physicsvehiclehandling.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / gxMessageFilterFlags

#include <cmath>   // fabsf -- the console's `vandc` sign-bit clear on the wheel-size compare
#include <cstdlib> // getenv -- the [T2-attrib] diag gate only

namespace BrnTraffic
{
namespace
{
    // The console's own wheel count -- the literal the inlined
    // StreamedDeformationSpec::GetWheelSpec bounds-check compares against
    // ("liWheel < eNumWheels", `cmpwi r30, 4` @0x82761BF4).
    const s32 KI_NUM_WHEELS = 4;

    // unk_820C0B90, the tolerance on the "Traffic wheels must all be the same size" assert
    // (.cpp:115). A plain .rdata literal, bytes `37 80 00 00` == 1.52588e-05f == 2^-16. The
    // three lanes following it are the shared 0.0174533 / 57.2958 / pi trio.
    const f32 KF_WHEEL_SIZE_EPSILON = 1.52588e-05f;

    // [T1-traffic-leg] one-shot gate log. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    void LogMissingLeg( bool& lrbAlreadyLogged, const char* lpcLegNameAndReason )
    {
        if ( lrbAlreadyLogged )
        {
            return;
        }
        lrbAlreadyLogged = true;

        if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 && CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[T1-traffic-leg] VehicleTypeRuntime::Prepare leg NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndReason << " [FLAG PC partial gate]\n";
        }
    }
}

// VehicleTypeRuntime::Prepare @0x82761B10 *** PARTIAL ***
// DWARF :66 `void Prepare(const StreamedDeformationSpec*, Attribute::Key)`; the second
// parameter takes the console's real 8-byte width (see the header's note).
//
// Called once per vehicle TYPE from TrafficEntityModule::LoadData @0x82746A88 stage 7, the
// moment that type's physics/deformation spec arrives.
void VehicleTypeRuntime::Prepare(
    const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec,
    AttribKey lAttribKey )
{
    CGS_ASSERT( lpSpec != 0, "lpSpec" );

    // 0x82761B3C `std r4, 0x40(r16)`. The console stores the key before it does anything else
    // with it; the next instruction hands the same register to Attrib::Instance. Order kept.
    mAttribKey = lAttribKey;

    // ---- 0x82761B40..0x82761B84: the attrib chase --------------------------------------
    // burnoutcarasset(key,0) -> RefSpec at dataArea+0x158 -> physicsvehiclehandling ->
    // VehicleAttribs::Construct + SetupAttribs. DWARF names the locals at .cpp:96/:97/:98.
    // The by-value SetupAttribs argument goes through the out-of-line copy ctor @0x825BDB88.
    BrnPhysics::Vehicle::VehicleAttribs lVehicleAttribs;
    Attrib::Gen::burnoutcarasset lBaseCarAsset( mAttribKey, NULL );
    Attrib::Gen::physicsvehiclehandling lHandling(
        const_cast<Attrib::Collection*>(
            lBaseCarAsset.GetPhysicsVehicleHandlingRefSpec()->GetCollection() ), NULL );
    lVehicleAttribs.Construct();
    {
        // Only the by-value copy is scoped; lHandling and lBaseCarAsset stay alive to the end
        // of the function, which is the console's destruction order (0x82762074..0x82762088).
        Attrib::Gen::physicsvehiclehandling lHandlingCopy( lHandling );   // @0x825BDB88
        lVehicleAttribs.SetupAttribs( lHandlingCopy );
    }

    // ---- the four wheel placements (0x82761BF4..0x82761D34, `do { } while (v8 < 4)`) -----
    // The console builds two 4-element stack arrays inside the loop: the wheel POSITIONS
    // (from the inlined GetWheelSpec) and the wheel RADII, each radius being half the wheel
    // spec's mScale lane 1 (`vspltw v12, v12, 1` then `vmulfp128 v0, v12, splat(0.5f)`). The
    // 0.5f is a rodata read, `0x82761BA0 lfs f31, flt_82001DA0@l(r10)`, splatted through the
    // stack; Hex-Rays renders it as an immediate stack constant after folding the load.
    Vector3 laWheelPositions[KI_NUM_WHEELS];
    f32     lafWheelRadii[KI_NUM_WHEELS];

    for ( s32 liWheel = 0; liWheel < KI_NUM_WHEELS; liWheel++ )
    {
        // De-inlined GetWheelSpec: the console inlines it, which is why its own
        // "liWheel < eNumWheels" assert string appears in this function's literal pool.
        const BrnPhysics::Deformation::WheelSpec* lpWheel =
            lpSpec->GetWheelSpec( liWheel );

        laWheelPositions[liWheel] = lpWheel->mPosition;
        lafWheelRadii[liWheel]    = 0.5f * lpWheel->mScale.y;

        // The wheel-size equality tripwire. The console emits it INSIDE the wheel loop, once
        // per wheel (0x82761C54..0x82761D2C), which is why it sits here and not after:
        //     vsubfp128 v0, v127, v0        ; lafWheelRadii[0] - lafWheelRadii[liWheel]
        //     vandc128  v0, v0, v126        ; v126 == vslw(-1,-1) == 0x80000000 splat -> fabs
        //     vcmpgtfp  v0, v0, v13         ; v13 == vspltw(lvlx(unk_820C0B90), 0)
        //     -> FireAssert("Traffic wheels must all be the same size",
        //        BrnTrafficVehicleTypeRuntime.cpp, 115)            ; `li r5, 0x73` == 115
        // Iteration 0 compares wheel 0 against itself, so the difference is exactly 0. That
        // self-comparison is the console's, not an artefact.
        //
        // BEHAVIOUR: this released assert is LIVE. If a shipped B5TRAFFIC.BNDL vehicle type
        // carries mismatched wheel scales it will fire on the next boot, which is correct --
        // the console fires there too, and mMass_WheelRadius_Z_W.y below takes wheel 0's
        // radius as THE radius for the type. A fire is data news; do not re-gate it.
        CGS_ASSERT( fabsf( lafWheelRadii[0] - lafWheelRadii[liWheel] ) <= KF_WHEEL_SIZE_EPSILON,
                    "Traffic wheels must all be the same size" );   // .cpp:115
    }

    // ---- 0x82761E64ff: the wheel radius lane (vrlimi128 v12, v127, 4, 0 -> word 1) -------
    // v127 is the FIRST wheel's radius; the loop leaves it loaded from element 0. Wheel 0's
    // radius is THE radius for the type, which is what the equality assert above justifies.
    mMass_WheelRadius_Z_W.y = lafWheelRadii[0];

    // ---- the two axle lanes (see the vrlimi analysis in the file banner) -----------------
    // FwdAxle  (.w, mask 1) <- maWheelSpecs[1].mPosition longitudinal lane   (a FRONT wheel)
    // BackAxle (.z, mask 2) <- maWheelSpecs[3].mPosition longitudinal lane   (a REAR wheel)
    // .x (CabPivot) and .y (TrailerPivot) are written later in the console body, by the
    // mGenericTags locator walk (GATED LEG 3 below).
    mCabPivot_TrailerPivot_BackAxle_FwdAxle.w = laWheelPositions[1].z;
    mCabPivot_TrailerPivot_BackAxle_FwdAxle.z = laWheelPositions[3].z;

    // ---- 0x82761EA4 / 0x82761EAC: the two box lanes --------------------------------------
    //   lvx128 v0, r28, 1680 ; stvx128 v0, r0,  r16      this+0x00 <- spec + 1680
    //   lvx128 v0, r28, 64   ; stvx128 v0, r16, r27(16)  this+0x10 <- spec + 64
    // TrafficEntityModule::Prepare stage 3 reads both to build each vehicle type's
    // rw::collision::BoxVolume.
    mBBoxOffset   = lpSpec->mCollisionOffset;
    mBBoxHalfSize = lpSpec->mHandlingBodyDimensions;

    // ---- 0x82761DD0 assert (.cpp:125) then 0x82761E24 vrlimi128 v12, v13, 8, 0 -> .x ----
    // DWARF calls the local lfMass (.cpp:126) and reads it through
    // VehicleAttribs::VehicleBaseAttribs::GetMass, which is base+0x70 lane 0. The console
    // stack proves the offset: VehicleAttribs sits at sp+0x190 and the splat loads sp+0x200.
    // This released assert is LIVE; a fire on shipped data is data news, do not re-gate it.
    const f32 lfMass =
        lVehicleAttribs.mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;
    CGS_ASSERT( lfMass > 0.0f, "lVehicleAttribs.mBaseAttribs.GetMass() > 0.0f" );   // .cpp:125
    mMass_WheelRadius_Z_W.x = lfMass;

    {
        // GATED LEG 3 -- the LOCATOR walk over mGenericTags (0x82761E00..0x82761E60).
        //
        // The console walks mGenericTags' muNumLocators locators for tag types 28 and 29,
        // transforms each hit's translation by a matrix composed from
        // mCarModelSpaceToHandlingBodySpaceTransform (spec + 1552) and the negated COM row,
        // and vrlimi's the results into this+0x20:
        //     tag 29: 0x82761F24 vrlimi128 v13, v0, 8, 2   mask 8 -> word 0 -> .x mCabPivot
        //     tag 28: 0x82761F88 vrlimi128 v13, v0, 4, 1   mask 4 -> word 1 -> .y mTrailerPivot
        //     both:   0x82761F8C stvx128   v13, r0, r29    (r29 == this + 0x20)
        //
        // BLOCKERS: (1) the composed matrix comes out of a vmrghw/vmrglw transpose plus a
        // vsubfp negation of one row, a lane layout this cluster will not guess; (2) tag ids
        // 28 and 29 have no named enumerator in this tree's ETagPointType.
        //
        // CONSEQUENCE: the axle lanes above use masks 1 and 2 (words 3 and 2), disjoint from
        // this walk's words 0 and 1, so they are unaffected. But on any vehicle type carrying
        // a type-28/29 locator, mCabPivot and mTrailerPivot keep Construct's {-1.5f, 1.5f}
        // placeholder. The live reader is BrnTrafficVehicle.cpp:511
        // (`- lpVehicleTypeRuntime->GetTrailerPivotDistance()`), so a trailered type's
        // articulation point sits at the placeholder until this walk lands.
        static bool sbLogged = false;
        LogMissingLeg( sbLogged,
            "VehicleTypeRuntime::Prepare mGenericTags locator walk (tag types 28/29, .h:98 "
            "\"luTag < muNumLocators\") -- it composes a matrix by vmrghw/vmrglw transpose of "
            "mCarModelSpaceToHandlingBodySpaceTransform against a negated COM row and this "
            "cluster will not guess a transpose (see the shadow wave). It writes .x mCabPivot "
            "(tag 29) and .y mTrailerPivot (tag 28) -- NOT the axle lanes -- so types WITHOUT "
            "such a locator are unaffected and types with one keep Construct's {-1.5f, 1.5f} "
            "placeholder, which BrnTrafficVehicle.cpp:511 reads via GetTrailerPivotDistance()" );
    }

    // ---- 0x82761FA8..0x82762070: the random-traffic paint palette (DWARF .cpp:176/:179) ----
    // burnoutcargraphicsasset off the RefSpec at dataArea+0x170, then
    // miNumPaintColours = min( Num_RandomTrafficColours(), 20 ) and one s8 per entry. The
    // console's branchless min (srawi/not/and/andi/or) is the structured min; the element read
    // is `lwz r10,0(r3) ; stb r10,0x48(r9)` -- the Int32 value truncated to its low byte, so it
    // is a value truncation on the host, not a first-byte read.
    {
        Attrib::Gen::burnoutcargraphicsasset lGraphicsAsset(
            const_cast<Attrib::Collection*>(
                lBaseCarAsset.GetGraphicsAssetRefSpec()->GetCollection() ), NULL );

        const s32 liNumColours = static_cast<s32>( lGraphicsAsset.Num_RandomTrafficColours() );
        miNumPaintColours = static_cast<s8>(
            liNumColours < static_cast<s32>( KU_NUM_PAINT_COLOURS_PER_VEHICLE )
                ? liNumColours
                : static_cast<s32>( KU_NUM_PAINT_COLOURS_PER_VEHICLE ) );

        for ( u8 luTrafficColourIndex = 0; luTrafficColourIndex < miNumPaintColours;
              luTrafficColourIndex++ )
        {
            maiPaintColours[luTrafficColourIndex] = static_cast<s8>(
                lGraphicsAsset.RandomTrafficColours( luTrafficColourIndex ) );
        }
    }

    // [T2-attrib] one-shot: the first attrib seat. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    if ( getenv( "BRN_TRAFFIC_DIAG" ) != 0 )
    {
        static bool sbDiagLogged = false;
        if ( !sbDiagLogged )
        {
            sbDiagLogged = true;
            if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 && CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[T2-attrib] VehicleTypeRuntime::Prepare seated mass*1000="
                    << static_cast<s32>( lfMass * 1000.0f )
                    << " paintColours=" << static_cast<s32>( miNumPaintColours )
                    << " attribKeyLo=" << static_cast<s32>( mAttribKey & 0xFFFFFFFFu ) << "\n";
            }
        }
    }
}

}
