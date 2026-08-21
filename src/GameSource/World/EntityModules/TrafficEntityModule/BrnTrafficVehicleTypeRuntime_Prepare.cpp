// ============================================================================
// BrnTrafficVehicleTypeRuntime_Prepare.cpp -- wave T1 (parked traffic cars) round 3,
// CLOSURE item 3.
//
// ONE FUNCTION LIVES HERE: BrnTraffic::VehicleTypeRuntime::Prepare @0x82761B10.
//
// ---------------------------------------------------------------------------
// ⭐⭐ WHY THIS IS THE BOOT-BLOCKING FRONTIER, MEASURED NOT PREDICTED.
//
// The rounds-1+2 build boots with BRN_TRAFFIC_DIAG=1 and logs
//     [T1-scene] FIRST traffic volume registered: volumeId=36 typeFlag=8
//                halfExtents=(0,0,0) fatness=0 of 30 vehicle types.
//                ZERO EXTENTS MEAN VehicleTypeRuntime::Prepare NEVER RAN
// followed by 90 released asserts -- "Bounding box with zero width/height/depth for volume
// instance id" (CgsVolumeManager.cpp:221/223/225), three per vehicle type, thirty types --
// and the run never leaves BOOT. TrafficEntityModule::Prepare stage 3's AddDynamicVolume
// loop is live and registering thirty DEGENERATE boxes, because
// maVehicleTypeRuntime[type].mBBoxHalfSize still holds VehicleTypeRuntime::Construct's
// SetZero(). This function is what fills it.
//
// (Note for the shadow-wave reader: this is the KF_DEFAULT_ASPECTRATIO failure SHAPE -- a
// placeholder zero producing geometry that is degenerate but never non-finite -- except the
// volume manager happens to own a tripwire for it, so it screams instead of going silent.)
//
// ---------------------------------------------------------------------------
// SOURCE LADDER. Rung 1 only, and it is complete for what lands here: the ARTIST
// pseudocode + assembly at 0x82761B10 (a 330-instruction VMX body). DecFIGS supplies the
// declaration shape (BrnTrafficVehicleTypeRuntime.h:66) and the member names
// (:94..:112). Feb-2007 has no VehicleTypeRuntime::Prepare at all -- the type predates it
// only as the three members Construct seeds -- so rung 3 is empty and rung 1 arbitrates
// alone.
//
// ---------------------------------------------------------------------------
// THE SEVEN STORES THIS FUNCTION MAKES, and the state of each here.
//
//   asm                                    member                        state
//   ------------------------------------   ---------------------------   -----
//   std  r4, 0x40(r16)                     mAttribKey                    ⭐ REAL
//   lvx  v0,r28,1680 ; stvx v0,r0,r16      mBBoxOffset      (this+0x00)  ⭐ REAL
//   lvx  v0,r28,64   ; stvx v0,r16,16      mBBoxHalfSize    (this+0x10)  ⭐ REAL  <- the boot fix
//   vrlimi v12,v0,1,3   -> this+0x20 .w    mCabPivot_..._FwdAxle         ⭐ REAL
//   vrlimi v0, v13,2,0  -> this+0x20 .z    mCabPivot_..._BackAxle        ⭐ REAL
//   vrlimi v12,v127,4,0 -> this+0x30 .y    mMass_WheelRadius_Z_W.y       ⭐ REAL (wheel radius)
//   vrlimi v12,v13, 8,0 -> this+0x30 .x    mMass_WheelRadius_Z_W.x       GATED (VehicleAttribs)
//   stb  r?, 0x5C(r16) + 0x48+i            miNumPaintColours / maiPaint  GATED (attrib table)
//
// ⭐ CORRECTED 2026-08-21 (round 3 FIX pass). What stood here said "mCabPivot (.x) and
// mTrailerPivot (.y) are NOT written by this function on the console either -- they keep
// Construct's {-1.5f, 1.5f, ...} seed ... Reproduced, not 'completed'." THAT IS FALSE, and it
// is the sentence that would stop the next reader from looking. The console DOES write both
// lanes in this function, in the gated mGenericTags locator walk, for any vehicle type that
// carries a type-28 or type-29 locator:
//
//   asm                                    member                        state
//   ------------------------------------   ---------------------------   -----
//   vrlimi v13,v0, 8,2 @0x82761F24         .x mCabPivot     (tag 29)     GATED (locator walk)
//   vrlimi v13,v0, 4,1 @0x82761F88         .y mTrailerPivot (tag 28)     GATED (locator walk)
//   stvx128 v13,r0,r29 @0x82761F8C         ...both into this+0x20 (r29 = this+0x20,
//                                             set at 0x82761D50 `addi r29, r16, 0x20`)
//
// mask 8 == word 0 == .x and mask 4 == word 1 == .y, which are DISJOINT from the axle stores'
// masks 2/1 (words 2/3 == .z/.w). So the real consequence of gating the walk is that
// mCabPivot / mTrailerPivot keep Construct's {-1.5f, 1.5f} PLACEHOLDER on locator-carrying
// types -- see GATED LEG 3 for the live reader that sees it.
//
// ---------------------------------------------------------------------------
// HOW THE SPEC OFFSETS WERE RESOLVED (no raw offsets survive into the code).
// r28 is the incoming `const StreamedDeformationSpec*`; every byte offset the asm uses maps
// onto a NAMED member through the layout contract already pinned by static_assert in
// BrnStreamedDeformationSpec.h:
//     spec + 64            mHandlingBodyDimensions   (static_assert "@ +64")
//     spec + 80 + 48*i     maWheelSpecs[i].mPosition (static_assert "@ +80", stride 48)
//     spec + 96 + 48*i     maWheelSpecs[i].mScale    (the same pair, +16 into the element)
//     spec + 1552          mCarModelSpaceToHandlingBodySpaceTransform (static_assert)
//     spec + 1632/1648     mCurrentCOMOffset / mMeshOffset            (static_assert)
//     spec + 1664/1680     mRigidBodyOffset / mCollisionOffset        (the same 16-byte run)
//     spec + 36            mGenericTags.muNumLocators / mpaLocatorPoints (LocatorPointSpecList)
// The three static_asserts at 1552/1632/1648 fix the tail run's phase exactly, so +1680 is
// mCollisionOffset with no slack, and the locator walk at +36 lands on mGenericTags.
//
// ---------------------------------------------------------------------------
// THE VMX LANE MERGES, SPELLED OUT (this is where a reconstruction goes quietly wrong).
// `vrlimi128 vD, vS, mask, sh` rotates vS and inserts the mask-selected WORDS into vD. mask
// is 8/4/2/1 for words 0/1/2/3. The two pivot writes are:
//     vrlimi128 v12, v0,  1, 3   with v0  = maWheelSpecs[1].mPosition
//     vrlimi128 v0,  v13, 2, 0   with v13 = maWheelSpecs[3].mPosition
// mask 1 is word 3 (.w == FwdAxle) and mask 2 is word 2 (.z == BackAxle). The rotate counts
// (3 and 0) put the SAME source lane -- word 2, i.e. the wheel's mPosition.z -- into both,
// which is the reading that makes the pair self-consistent: the forward axle offset comes
// from a FRONT wheel (index 1) and the back axle offset from a REAR wheel (index 3), both
// read out of the wheel's longitudinal lane. Two independent halves agreeing on one lane is
// the check; a lane-mapping error would have made them disagree.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / gxMessageFilterFlags

#include <cmath>   // fabsf -- the console's `vandc` sign-bit clear on the wheel-size compare

namespace BrnTraffic
{
namespace
{
    // The console's own wheel count -- the literal the inlined
    // StreamedDeformationSpec::GetWheelSpec bounds-check compares against
    // ("liWheel < eNumWheels", `cmpwi r30, 4` @0x82761BF4).
    const s32 KI_NUM_WHEELS = 4;

    // ⭐ RECOVERED 2026-08-21 -- unk_820C0B90, the tolerance on the "Traffic wheels must all
    // be the same size" assert (.cpp:115). Bytes `37 80 00 00` == 1.52588e-05f == 2^-16, read
    // straight out of the .i64 with ida_bytes.get_bytes(0x820C0B90, 16); the three lanes that
    // follow it in the same .rdata block are the shared 0.0174533 / 57.2958 / pi trio, which
    // corroborates the address. NOT a dyn-init thunk global -- it is a plain .rdata literal,
    // which is why the 0x82C6xxxx thunk-walk recipe in recovered_constants.md does not apply
    // to it and why two earlier rounds could not find it. Its only other readers in the image
    // are DebugComponent::DrawAxle @0x8275BBC0 and DrawAvoidance @0x8275BE28.
    const f32 KF_WHEEL_SIZE_EPSILON = 1.52588e-05f;

    // Same PARTIAL-pattern one-shot leg gate the sibling traffic partfiles use, so one boot
    // log reads as a single stream. [DIAG] NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
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

// ----------------------------------------------------------------------------
// VehicleTypeRuntime::Prepare  @ 0x82761B10   *** PARTIAL ***
//
// DWARF :66 `void Prepare(const StreamedDeformationSpec*, Attribute::Key)`; the second
// parameter takes the console's real 8-byte width here (see the header's own note).
//
// Called once per vehicle TYPE from TrafficEntityModule::LoadData @0x82746A88 stage 7, the
// moment that type's physics/deformation spec arrives:
//     maTrafficVehiclePhysicsSpecs[type] = reply.mHandle;
//     lpSpec     = maTrafficVehiclePhysicsSpecs[type].operator->();
//     lAttribKey = FindVehicleTypeAttribKey_EXPENSIVE(type);
//     maVehicleTypeRuntime[type].Prepare(lpSpec, lAttribKey);
// ----------------------------------------------------------------------------
void VehicleTypeRuntime::Prepare(
    const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec,
    AttribKey lAttribKey )
{
    CGS_ASSERT( lpSpec != 0, "lpSpec" );

    // ---- 0x82761B3C `std r4, 0x40(r16)` -------------------------------------------------
    // The console stores the key BEFORE it does anything else with it (the very next
    // instruction hands the same register to the Attrib::Instance constructor). Order kept.
    mAttribKey = lAttribKey;

    {
        // GATED LEG 1 -- the ATTRIB half of the function, both ends of it.
        //
        // The console opens with
        //     Attrib::Instance lInstance( lAttribKey, 0 );                       sub_82204998
        //     Attrib::Gen::physicsvehiclehandling lHandling(
        //         Attrib::RefSpec::GetCollection( lInstance.<+344> ), 0 );
        //     BrnPhysics::Vehicle::VehicleAttribs lVehicleAttribs;
        //     lVehicleAttribs.Construct();
        //     lVehicleAttribs.SetupAttribs( sub_825BDB88( lHandling ) );
        // and closes with
        //     Attrib::Gen::burnoutcargraphicsasset lGraphics(
        //         Attrib::RefSpec::GetCollection( lInstance.<+368> ), 0 );
        //     miNumPaintColours = min( 20, Attrib::Attribute::GetLength(
        //         Attrib::Instance::Get( lGraphics, -1309769630 ) ) );
        //     for (i < miNumPaintColours)
        //         maiPaintColours[i] = *Attrib::Instance::GetAttributePointer(
        //                                   lGraphics, -1309769630, i );
        // Between them it uses the VehicleAttribs ONLY for the mass:
        //     CGS_ASSERT( lVehicleAttribs.mBaseAttribs.GetMass() > 0.0f, ... )   (.cpp:125)
        //     mMass_WheelRadius_Z_W.x = mass                                     vrlimi ...,8,0
        //
        // NOT LANDED, and the reason is a dependency chain, not an evidence gap: the whole
        // leg needs Attrib::Instance's key constructor (sub_82204998), Attrib::RefSpec::
        // GetCollection, the two GENERATED attrib classes, BrnPhysics::Vehicle::VehicleAttribs
        // (Construct + SetupAttribs + mBaseAttribs) and sub_825BDB88 -- none of which has a
        // usable declaration reachable from this header's include graph, and pulling them in
        // would drag the physics-attribs subsystem into a traffic leaf TU.
        //
        // CONSEQUENCE, STATED PLAINLY, BOTH HALVES:
        //  (1) mMass_WheelRadius_Z_W.x keeps whatever the module's storage holds. Every reader
        //      of GetMass() is a wave-3 PHYSICAL-traffic leg and every one of them is still
        //      gated, so nothing reads it on the parked path.
        //  (2) miNumPaintColours keeps whatever the module's storage holds -- and
        //      PickPaintColourForVehicle @0x827049A8 asserts `miNumPaintColours != 0` and
        //      indexes maiPaintColours with it. Its ONE consumer is
        //      BrnTrafficEntityModule_Render.cpp's per-car paint pick. ⚠️ If that render leg
        //      is ever un-gated before this one lands, it will index a garbage table. NOT
        //      "fixed" here by seeding a value: a fabricated colour count is exactly the
        //      placeholder-constant class this wave was told to hunt.
        static bool sbLogged = false;
        LogMissingLeg( sbLogged,
            "VehicleTypeRuntime::Prepare attrib half -- Attrib::Instance(key)/RefSpec::"
            "GetCollection, Attrib::Gen::physicsvehiclehandling + BrnPhysics::Vehicle::"
            "VehicleAttribs (the mMass_WheelRadius_Z_W.x mass seat) and Attrib::Gen::"
            "burnoutcargraphicsasset (the maiPaintColours/miNumPaintColours table, attrib id "
            "-1309769630). No declarations reachable from this TU. mBBoxOffset/mBBoxHalfSize/"
            "the axle lanes/the wheel radius ARE seated -- only mass and the paint table are "
            "skipped" );
    }

    // ---- the four wheel placements (0x82761BF4..0x82761D34, `do { } while (v8 < 4)`) -----
    // The console builds two 4-element stack arrays inside the loop: the wheel POSITIONS
    // (from the inlined GetWheelSpec) and the wheel RADII, each radius being half the wheel
    // spec's mScale lane 1 (`vspltw v12, v12, 1` then `vmulfp128 v0, v12, splat(0.5f)`).
    // PROVENANCE CORRECTED 2026-08-21 (round 3 FIX pass): the 0.5f IS a rodata read --
    // `0x82761BA0 lfs f31, flt_82001DA0@l(r10)`, loaded into f31 in the prologue and splatted
    // through the stack. The old comment called it "the console's own immediate stack constant
    // v55 = 0.5, not a rodata read", which is how Hex-Rays renders it after constant folding.
    // The VALUE is unchanged and correct -- Hex-Rays folds the load to the literal `v55 = 0.5`,
    // which it can only do by reading flt_82001DA0's bytes out of the image; only the sourcing sentence was
    // wrong, and it mattered because "not a rodata read" would stop a future agent from dumping
    // the literal if the halving were ever doubted.
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

        // ⭐⭐ LEG 2 UN-GATED 2026-08-21 (wave T1 round 4, item 3) -- the wheel-size equality
        // tripwire, with its REAL epsilon.
        //
        // The console emits this comparison INSIDE the wheel loop, once per wheel
        // (0x82761C54..0x82761D2C, between the GetWheelSpec bounds assert and the loop's
        // `addi r30,r30,1`), which is why it lives here rather than after the loop:
        //     vsubfp128 v0, v127, v0        ; lafWheelRadii[0] - lafWheelRadii[liWheel]
        //     vandc128  v0, v0, v126        ; v126 == vslw(-1,-1) == 0x80000000 splat -> fabs
        //     vcmpgtfp  v0, v0, v13         ; v13 == vspltw(lvlx(unk_820C0B90), 0)
        //     -> if the compare is true, FireAssert("Traffic wheels must all be the same
        //        size", BrnTrafficVehicleTypeRuntime.cpp, 115)     ; `li r5, 0x73` == 115
        // Iteration 0 compares wheel 0 against itself, so the difference is exactly 0 and the
        // assert cannot fire on it -- that self-comparison is the console's, not an artefact.
        //
        // ⭐ EPSILON RECOVERED (round-3 addition to scratchpad traffic_wave/
        // recovered_constants.md, epsilon_dump.txt): unk_820C0B90 == 1.52588e-05f == 2^-16,
        // bytes `37 80 00 00`. PROVENANCE, spelled out because round 3 parked this on exactly
        // the wrong recovery recipe: it is a PLAIN .rdata LITERAL, not one of the unnamed
        // 0x82C6xxxx dyn-init thunk globals, so the thunk walk does not apply to it -- it
        // needed one `ida_bytes.get_bytes(0x820C0B90, 16)` in the .i64, which is how it was
        // dumped. The three lanes that follow it are the shared 0.0174533 / 57.2958 / pi
        // block, which is the corroboration that the dump landed on the right address.
        // 2^-16 is also the value the shape argues for: a tolerance on a metres-scale wheel
        // radius, exact in binary, i.e. a deliberate constant rather than a rounded decimal.
        //
        // ⚠️ IT IS A RELEASED ASSERT AND IT IS NOW LIVE. If a shipped B5TRAFFIC.BNDL vehicle
        // type really does carry mismatched wheel scales, this WILL fire on the next boot --
        // and that is the correct outcome, because the console fires there too and because
        // mMass_WheelRadius_Z_W.y below takes wheel 0's radius as THE radius for the type.
        // A fire here is data news, not a regression; do not re-gate it.
        CGS_ASSERT( fabsf( lafWheelRadii[0] - lafWheelRadii[liWheel] ) <= KF_WHEEL_SIZE_EPSILON,
                    "Traffic wheels must all be the same size" );   // .cpp:115
    }

    // ---- 0x82761E64ff: the wheel radius lane (vrlimi128 v12, v127, 4, 0 -> word 1) -------
    // v127 is the FIRST wheel's radius (the loop leaves it loaded from element 0). The
    // console takes wheel 0's radius as THE radius for the type, which is exactly what the
    // gated equality assert above is there to justify.
    mMass_WheelRadius_Z_W.y = lafWheelRadii[0];

    // ---- the two axle lanes (see the vrlimi analysis in the file banner) -----------------
    // FwdAxle  (.w, mask 1) <- maWheelSpecs[1].mPosition longitudinal lane   (a FRONT wheel)
    // BackAxle (.z, mask 2) <- maWheelSpecs[3].mPosition longitudinal lane   (a REAR wheel)
    // .x (CabPivot) and .y (TrailerPivot) are written LATER in the console body, by the
    // mGenericTags locator walk (GATED LEG 3 below) -- not "untouched", as this comment used
    // to say. They keep Construct's {-1.5f, 1.5f} seed only because that walk is gated, and
    // only on types that carry a type-28/29 locator.
    mCabPivot_TrailerPivot_BackAxle_FwdAxle.w = laWheelPositions[1].z;
    mCabPivot_TrailerPivot_BackAxle_FwdAxle.z = laWheelPositions[3].z;

    // ---- 0x82761EA4 / 0x82761EAC: the two box lanes -- THE BOOT FIX ----------------------
    //   lvx128 v0, r28, 1680 ; stvx128 v0, r0,  r16      this+0x00 <- spec + 1680
    //   lvx128 v0, r28, 64   ; stvx128 v0, r16, r27(16)  this+0x10 <- spec + 64
    // spec+64 is mHandlingBodyDimensions (static_assert-pinned in the spec header) and
    // spec+1680 is mCollisionOffset, the fourth entry in the 16-byte-strided tail run whose
    // phase is fixed by the mCurrentCOMOffset@1632 / mMeshOffset@1648 pins.
    //
    // These two are what TrafficEntityModule::Prepare stage 3 reads to build each vehicle
    // type's rw::collision::BoxVolume. Until this store existed, stage 3 registered thirty
    // zero-extent boxes and CgsVolumeManager fired three asserts per type.
    mBBoxOffset   = lpSpec->mCollisionOffset;
    mBBoxHalfSize = lpSpec->mHandlingBodyDimensions;

    {
        // GATED LEG 3 -- the LOCATOR walk over mGenericTags (0x82761E00..0x82761E60).
        //
        // The console walks mGenericTags' muNumLocators locators looking for tag types 28 and
        // 29, transforms each hit's translation by the matrix it composed from
        // mCarModelSpaceToHandlingBodySpaceTransform (spec + 1552) and the negated COM row,
        // and vrlimi's the results into the two REMAINING lanes (.x/.y) of
        // mCabPivot_TrailerPivot_BackAxle_FwdAxle, the same this+0x20 vector whose .z/.w the
        // axle stores above fill.
        //
        // NOT LANDED. Two reasons, both real:
        //  (1) the composed matrix comes out of a vmrghw/vmrglw TRANSPOSE of the spec's
        //      handling-body transform combined with a vsubfp negation of one row -- a lane
        //      layout this cluster is not confident reproducing, and the shadow-system wave's
        //      transposed-rotation-pair bug is the standing reminder of what a wrong transpose
        //      costs (a silently mirrored offset, never a non-finite value);
        //  (2) the two tag-point type ids (28, 29) have no named enumerator in this tree's
        //      ETagPointType, whose reconstructed run does not obviously cover them.
        // ⭐ CONSEQUENCE STATEMENT CORRECTED 2026-08-21 (round 3 FIX pass). What stood here --
        // "the lanes it would write are the SAME lanes the axle stores above cover ... types
        // with one keep the wheel-derived axle offset rather than the locator-derived one" --
        // NAMED THE WRONG LANES. The walk writes words 0 and 1 of this+0x20:
        //     tag 29: 0x82761F24 vrlimi128 v13, v0, 8, 2   mask 8 -> word 0 -> .x mCabPivot
        //     tag 28: 0x82761F88 vrlimi128 v13, v0, 4, 1   mask 4 -> word 1 -> .y mTrailerPivot
        //     both:   0x82761F8C stvx128   v13, r0, r29    (r29 == this + 0x20)
        // The axle stores landed above are masks 1 and 2 -- words 3 and 2, .w and .z. Disjoint
        // word sets, so the axle offsets this file lands are NOT affected by this gate at all.
        //
        // THE REAL OBSERVABLE CONSEQUENCE: on any vehicle type carrying a type-28/29 generic
        // locator, mCabPivot (.x) and mTrailerPivot (.y) keep VehicleTypeRuntime::Construct's
        // {-1.5f, 1.5f} PLACEHOLDER instead of the locator-derived distance. The live reader is
        // BrnTrafficVehicle.cpp:511 (`- lpVehicleTypeRuntime->GetTrailerPivotDistance()`), the
        // only consumer of either lane in this tree -- so a trailered type's articulation point
        // sits at the placeholder until this walk lands.
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
}

}
