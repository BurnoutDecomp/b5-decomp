// Layout check for the BrnPhysics::Vehicle::SimpleVehiclePhysics and ::VehiclePhysics
// OWN-MEMBER BLOCKS (X360 +0x130..+0x720 and +0x720..+0x13F0).
//
// ⚠️⚠️ WHY THIS TU EXISTS -- READ BEFORE DELETING IT.
// BrnSimpleVehiclePhysics.h and VehiclePhysics.h now carry two recovered own-member blocks whose
// whole claim to being a DERIVATION rather than sixty separate guesses is that the DecFIGS DWARF's
// member ORDER and the X360 asm's member OFFSETS meet with zero slack at BOTH ends:
//     SimpleVehiclePhysics closes on 0x720 == VehiclePhysics::mpAttribs (Construct `stw r30,0x720`)
//     VehiclePhysics       closes on 0x13F0 == RaceCarPhysics::mPropCollisionImpulseSum, which a
//                          DIFFERENT wave derived from a DIFFERENT function and from which the
//                          RaceCarPhysics block closes on the 0x1460 per-car stride.
// A claim like that is worth exactly as much as the gate that checks it.
//
// ⭐⭐ WHY THIS GATE IS ARITHMETIC AND NOT `offsetof` -- and why that is the STRONGER choice here.
// The RaceCarPhysics own block could be gated with offsetof because it is width-identical on x64
// (no pointer, no vptr anywhere in it) and it is declared in DWARF order. NEITHER holds here:
//   * VehiclePhysics owns two POINTERS (mpAttribs @0x720, mpDebugComponent @0x13E4) which widen
//     4 -> 8 on the host, and SimpleVehiclePhysics's leaf vptr does the same;
//   * several embedded sub-types in this tree are RECONSTRUCTIONS, not byte-exact console copies
//     (SimpleVehicleAttribs is a 20-byte owning slice against the console's 240) -- deliberately,
//     per the project rule that parity is BY NAMED MEMBERS and unrecovered interiors are never
//     faked as padding;
//   * and VehiclePhysics.h is explicitly NOT in DWARF declaration order (it grew additively, group
//     by group), so even a RELATIVE host offsetof would not measure the console spacing.
// An `offsetof` gate here would therefore either be false or be a tautology. What this TU asserts
// instead is the thing the wave actually recovered: **that walking the DWARF member ORDER with the
// asm-literal sub-object SIZES reproduces every independently asm-literal ANCHOR, and lands exactly
// on 0x13F0.** Every number on the right-hand side of every assert below is an X360 literal quoted
// from the map in the two headers; none is computed from the left-hand side.
//
// ⚠️ THE BLIND SPOT, stated rather than hidden. This gate cannot see a member that occupies no
// space (an omitted trailing bool inside alignment padding), exactly as the RaceCarPhysics gate
// could not. What guards that is that the DWARF list is exhaustive and is quoted verbatim in the
// two headers, so a missing member would have to be deleted from a quoted list. The gate DOES fire
// for any size change, any reordering across an anchor, and any dropped non-padding member.
//
// TAMPER-TESTED 2026-08-03, THIRTEEN cases, **ten fire**:
//   FIRES  SweptSphere 0x20 -> 0x30 (breaks the whole SVP anchor chain at the first assert)
//   FIRES  EngineAttribs 0xA0 -> 0xB0
//   FIRES  BitArray treated as 4-aligned instead of 8 in the walk
//   FIRES  delete VehiclePhysics::mbContactingWall
//   FIRES  delete VehiclePhysics::mbInBoostKick (a bool the ARITHMETIC cannot see -- caught by the
//          named-member existence checks at the bottom, which is why they are there)
//   FIRES  insert an f32 inside SlamEffect before mi8SlamNumber
//   FIRES  swap SlamEffect::mfSteering / mfOriginalSteering
//   FIRES  delete AboveGroundTestResult::mfVerticalDistance
//   FIRES  move the record's meCarType by 4        (BrnVehicleManager_layout_check.cpp)
//   FIRES  move the record's mCrashNormal by 16    (BrnVehicleManager_layout_check.cpp)
//   SILENT drop one of the four bools FROM THE ARITHMETIC (0x10F5..0x10F8 is 11 bytes of pad)
//   SILENT append an f32 after SlamEffect::mi8SlamNumber (trailing pad)
//   SILENT append a bool after mpDebugComponent (0x13E8..0x13F0 is 8 bytes of pad)
// The three silent cases are the alignment-padding blind spot described above; each is covered
// instead by a named-member existence check, so the member cannot be DELETED silently.

#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"

#include <cstddef>   // offsetof, std::size_t

namespace BrnPhysics
{
namespace Vehicle
{
    // ==============================================================================================
    // PART 1 -- the CONSOLE arithmetic. Pure integer maths over the X360Layout literals; it does not
    // touch the host types at all, so it measures the RECOVERED LAYOUT and nothing else.
    // ==============================================================================================
    namespace X360LayoutCheck
    {
        using namespace X360Layout;

        // ---------- SimpleVehiclePhysics: 0x130 -> 0x720 ----------
        // The base chain ends at 0x130 (asm: SimpleVehiclePhysics::Construct `addi r3,r31,0x130`,
        // the address it hands to the first Wheel::Clear). DWARF BrnSimpleVehiclePhysics.h:357-373.
        const unsigned KU_A_WHEELS        = KU_SVP_BASE_END;                        // :357  0x130
        const unsigned KU_A_SPHERES       = KU_A_WHEELS   + 4u * KU_SVP_WHEEL_STRIDE;      // :358
        const unsigned KU_A_TRACTIONPTS   = KU_A_SPHERES  + 4u * KU_SVP_SWEPTSPHERE_SIZE;  // :359
        const unsigned KU_A_ABOVEGROUND   = KU_A_TRACTIONPTS + 4u * 0x10u;                 // :360
        static_assert(KU_A_ABOVEGROUND == KU_SVP_ABOVEGROUND_OFF,
                      "SVP: the wheel/sphere/traction arrays must land mAboveGroundTestResult on the "
                      "asm-literal 0x570 (Construct `addi r11,r31,0x570`)");

        const unsigned KU_A_SIMPLEATTRIBS = KU_A_ABOVEGROUND + KU_SVP_ABOVEGROUND_SIZE;    // :361
        static_assert(KU_A_SIMPLEATTRIBS == KU_SVP_SIMPLEATTRIBS_OFF,
                      "SVP: AboveGroundTestResult (pos,normal,f32,tag,bool -> 0x30) must land "
                      "mSimpleAttribs on the asm-literal 0x5A0 (`addi r3,r31,0x5A0`)");

        // sizeof(SimpleVehicleAttribs) on the console is NOT independently attested -- it is the gap
        // between two asm literals, which is exactly how it is written here, so this line is a
        // definition and not a check. It is the ONE console size in this block that is not
        // separately witnessed; flagged rather than dressed up.
        const unsigned KU_SVP_SIMPLEATTRIBS_SIZE =
            KU_SVP_HANDLINGBODYOFFSET - KU_SVP_SIMPLEATTRIBS_OFF;                          // 240
        static_assert(KU_SVP_SIMPLEATTRIBS_SIZE == 240u, "SVP: 0x690 - 0x5A0 == 240");

        const unsigned KU_A_HANDLINGBODY  = KU_A_SIMPLEATTRIBS + KU_SVP_SIMPLEATTRIBS_SIZE; // :362
        const unsigned KU_A_HALFEXTENT    = KU_A_HANDLINGBODY + 0x10u;                      // :363
        const unsigned KU_A_WHEELPLANE    = KU_A_HALFEXTENT   + 0x10u;                      // :364
        const unsigned KU_A_SPEEDMPH      = KU_A_WHEELPLANE   + 0x10u;                      // :365
        const unsigned KU_A_DEFORMAABB    = KU_A_SPEEDMPH     + 0x10u;                      // :366
        static_assert(KU_A_HALFEXTENT == 0x6A0u,
                      "SVP: mHalfExtent on the asm-literal 0x6A0 (Construct `li r8,0x6A0`)");
        static_assert(KU_A_SPEEDMPH == 0x6C0u,
                      "SVP: mfSpeedMPH on the asm-literal 0x6C0 (UpdateHandBrake `li r7,0x6C0`)");
        static_assert(KU_A_DEFORMAABB == KU_SVP_DEFORMABLE_AABB_OFF,
                      "SVP: mDeformableAABB on the asm-literal 0x6D0 (SetRaceCarCrashing's "
                      "ResetDeformableAABB copy DESTINATION)");

        const unsigned KU_A_ORIGINALAABB  = KU_A_DEFORMAABB   + KU_SVP_AABB_SIZE;           // :367
        static_assert(KU_A_ORIGINALAABB == KU_SVP_ORIGINAL_AABB_OFF,
                      "SVP: mOriginalAABB on the asm-literal 0x6F0 (the same copy's SOURCE)");

        const unsigned KU_A_MBCRASHING    = KU_A_ORIGINALAABB + KU_SVP_AABB_SIZE;           // :368
        static_assert(KU_A_MBCRASHING == KU_SVP_MBCRASHING_OFF,
                      "SVP: mbCrashing on 0x710 -- the seat the console's own assert string names "
                      "(FireAssert(\"mbCrashing\", \".../RaceCarPhysics.h\", 328))");

        // :369..:373 are five more bools -> last data byte 0x715, then 16-align.
        const unsigned KU_A_SVP_RAW_END   = KU_A_MBCRASHING + 6u;                           // 0x716
        const unsigned KU_A_SVP_SIZEOF    = (KU_A_SVP_RAW_END + 15u) & ~15u;
        static_assert(KU_A_SVP_SIZEOF == KU_SVP_SIZEOF,
                      "⭐ SVP CLOSURE: the six trailing bools must round the block to 0x720");

        // ---------- VehiclePhysics: 0x720 -> 0x13F0 ----------
        // DWARF VehiclePhysics.h:822-982, in declaration order.
        static_assert(KU_SVP_SIZEOF == KU_VP_MPATTRIBS_OFF,
                      "⭐ THE FIRST JOIN: sizeof(SimpleVehiclePhysics) must equal the asm-literal "
                      "offset VehiclePhysics::Construct stores mpAttribs at (`stw r30,0x720(r31)`). "
                      "Two functions, no shared assumption.");

        // mpAttribs (4 bytes on the console) + the 12 bytes of alignment the 16-aligned
        // VehicleAttribs forces.
        static_assert(KU_VP_AIATTRIBS_OFF == ((KU_VP_MPATTRIBS_OFF + 4u + 15u) & ~15u),
                      "VP: mAIVehicleAttribs must be the next 16-aligned slot after the 4-byte "
                      "mpAttribs (asm: `addi r3,r31,0x730`)");
        static_assert(KU_VP_PLAYERATTRIBS_OFF == KU_VP_AIATTRIBS_OFF + KU_VP_VEHICLEATTRIBS_SIZE,
                      "VP: the two VehicleAttribs Construct calls are 0x370 apart");

        const unsigned KU_B_SPRINGS       = KU_VP_PLAYERATTRIBS_OFF + KU_VP_VEHICLEATTRIBS_SIZE;
        static_assert(KU_B_SPRINGS == KU_VP_SPRINGS_OFF,
                      "⭐ VP: mPlayerVehicleAttribs + 0x370 must land maSprings on the asm-literal "
                      "0xE10 (Construct `addi r29,r31,0xE10`). Three asm literals, one arithmetic.");

        const unsigned KU_B_SPRINGSCALERS = KU_B_SPRINGS + 4u * KU_VP_SPRING_STRIDE;
        static_assert(KU_B_SPRINGSCALERS == KU_VP_SPRINGMASSSCALERS_OFF,
                      "VP: 4 springs at the asm-literal 0x30 stride must land mvSpringMassScalers "
                      "on the asm-literal 0xED0 (Construct `li r11,0xED0`)");

        const unsigned KU_B_WEIGHTXFER    = KU_B_SPRINGSCALERS + 0x10u;   // :830
        const unsigned KU_B_SPEEDONCRASH  = KU_B_WEIGHTXFER    + 0x10u;   // :831
        static_assert(KU_B_SPEEDONCRASH == KU_VP_SPEEDONLASTCRASH_OFF,
                      "VP: mWeightTransfer between them puts mvSpeedOnLastCrashMPH_... on the "
                      "asm-literal 0xEF0 (Construct `li r28,0xEF0`)");

        const unsigned KU_B_ENGINE        = KU_B_SPEEDONCRASH + 0x10u;    // :833
        static_assert(KU_B_ENGINE == KU_VP_ENGINE_OFF,
                      "VP: mEngine on the asm-literal 0xF00 (Construct `addi r30,r31,0xF00`)");

        // sizeof(Engine): mAttribs is the asm-literal 0xA0 (Engine::Prepare `memcpy(this,src,0xA0)`),
        // then two Vector4s put mu8CurrentGear at the asm-literal Engine+0xC0 (Prepare
        // `stw r10,0xC0(r31)`), then two bools -> 0xC6 -> 16-round -> 0xD0.
        static_assert(KU_VP_ENGINEATTRIBS_SIZE + 2u * 0x10u == KU_VP_ENGINE_GEAR_OFF,
                      "Engine: EngineAttribs(0xA0) + two Vector4s must land mu8CurrentGear on the "
                      "asm-literal Engine+0xC0");
        static_assert(((KU_VP_ENGINE_GEAR_OFF + 4u + 2u + 15u) & ~15u) == KU_VP_ENGINE_SIZE,
                      "Engine: mu8CurrentGear(u32) + two bools -> sizeof(Engine) == 0xD0");

        const unsigned KU_B_WHEELFRICTION = KU_B_ENGINE + KU_VP_ENGINE_SIZE;   // :835
        const unsigned KU_B_STEERINGANGLE = KU_B_WHEELFRICTION + 0x10u;        // :879
        static_assert(KU_B_STEERINGANGLE == KU_VP_STEERINGANGLE_OFF,
                      "⭐ VP: sizeof(Engine)==0xD0 plus mvfWheelFrictionLinearMultiplier must land "
                      "mvSteeringAngle_... on the asm-literal 0xFE0 (GetSteeringAngle "
                      "`addi r11,r31,0xFE0`, whose lane .x IS the steering angle)");

        // :879..:889 is ELEVEN Vector4s (mvSteeringAngle_... is :879 and is already placed), so ten
        // more steps of 0x10 reach the last of them. Together with the VecFloat at 0xFD0 the run is
        // twelve 16-byte registers, 0xFD0..0x1090.
        const unsigned KU_B_HANDBRAKETIMERS = KU_B_STEERINGANGLE + 10u * 0x10u;   // :889
        static_assert(KU_B_HANDBRAKETIMERS == KU_VP_HANDBRAKETIMERS_OFF,
                      "⭐ VP: the eleven-Vector4 bank must end on the asm-literal 0x1080 -- the "
                      "register UpdateHandBrake works lanes z/w of, which the DWARF member name "
                      "calls TimeHandbrakeHasBeenOn / TimeSinceLastHandBrake");

        const unsigned KU_B_PREVCONTROLS  = KU_B_HANDBRAKETIMERS + 0x10u;         // :905
        static_assert(KU_B_PREVCONTROLS + 0x44u == KU_VP_DRIVERTYPE_OFF,
                      "⭐ VP: mPreviousControls at 0x1090 puts its meDriverType (+0x44, a seat an "
                      "EARLIER wave pinned from BrnNetworkDriverControls::Clear) on the asm-literal "
                      "0x10D4 that VehiclePhysics::Prepare stores with `stw r30,0x10D4(r31)`");

        const unsigned KU_B_STEERINGDIR   = (KU_B_PREVCONTROLS + KU_VP_CONTROLS_SIZE + 15u) & ~15u;  // :906
        static_assert(KU_B_STEERINGDIR == 0x10E0u, "VP: mSteeringDirection @0x10E0");
        const unsigned KU_B_STUCKTIMER    = KU_B_STEERINGDIR + 0x10u;   // :907
        static_assert(KU_B_STUCKTIMER == KU_VP_STUCKTIMER_OFF,
                      "VP: mfTimeUntilStuckInCollisionTest on the asm-literal 0x10F0 "
                      "(UpdatePlayerStuckInCollisionSpheres `lfs`/`stfs 0x10F0`)");
        const unsigned KU_B_DRIFTFLAGS    = KU_B_STUCKTIMER + 4u;       // :910
        static_assert(KU_B_DRIFTFLAGS == KU_VP_DRIFTFLAGS_OFF,
                      "VP: mDriftFlags on the asm-literal 0x10F4 (ExitDrift `stb r4,0x10F4(r3)`)");

        // :912/:913/:916/:919 -- four bools -> 0x10F9 -> 16-round.
        const unsigned KU_B_SLAMEFFECT    = (KU_B_DRIFTFLAGS + 1u + 4u + 15u) & ~15u;   // :927
        static_assert(KU_B_SLAMEFFECT == KU_VP_SLAMEFFECT_OFF,
                      "⭐ VP: mDriftFlags + four bools must land mSlamEffect on 0x1100 -- the base "
                      "AddSlam addresses ALL SEVEN SlamEffect fields off, at the sub-struct's own "
                      "DWARF offsets (+0x14/+0x18 steering pair, +0x1C life, +0x20 total, +0x24 "
                      "recovery, +0x28 the slam number it clamps to KI8_MAX_NUM_SLAMS-1)");

        const unsigned KU_B_SHUNTEFFECT   = KU_B_SLAMEFFECT + 0x30u;   // :928
        static_assert(KU_B_SHUNTEFFECT == KU_VP_SHUNTEFFECT_OFF,
                      "VP: sizeof(SlamEffect)==0x30 lands mShuntEffect on the asm-literal 0x1130");
        const unsigned KU_B_LASTCONTACTED = KU_B_SHUNTEFFECT + 0x20u;  // :929
        static_assert(KU_B_LASTCONTACTED == KU_VP_LASTCONTACTED_OFF,
                      "VP: sizeof(ShuntEffect)==0x20 lands mi8LastContactedRaceCar on the "
                      "asm-literal 0x1150 (HandleRaceCarRaceCarContact `stb r14,0x1150(r16)`)");

        // BitArray<N> is one 64-bit field, so it is 8-ALIGNED: the s8 above leaves 7 bytes of pad.
        const unsigned KU_B_USEDAIRRAMS   = (KU_B_LASTCONTACTED + 1u + 7u) & ~7u;   // :932
        static_assert(KU_B_USEDAIRRAMS == KU_VP_USEDAIRRAMS_OFF,
                      "⭐ VP: mUsedAirRams on the asm-literal 0x1158 -- both the address Construct "
                      "zeroes with `std r30,0x1158(r31)` and the base UpdateAirRam walks with a "
                      "`ld`/`cntlzd` scan BOUNDED AT 4, which is the template argument");
        const unsigned KU_B_AIRRAMEFFECT  = (KU_B_USEDAIRRAMS + 8u + 15u) & ~15u;   // :933
        static_assert(KU_B_AIRRAMEFFECT == KU_VP_AIRRAMEFFECT_OFF,
                      "VP: mAirRamEffect[4] @0x1160");
        const unsigned KU_B_USEDSPINS     = KU_B_AIRRAMEFFECT + 4u * 0x30u;         // :935
        static_assert(KU_B_USEDSPINS == KU_VP_USEDSPINS_OFF,
                      "⭐ VP: four 0x30 AirRamEffects land mUsedSpins on the asm-literal 0x1220 -- "
                      "Construct's `std r30,0x1220(r31)` and UpdateSpinEffects' scan BOUNDED AT 8");
        const unsigned KU_B_SPINEFFECTS   = (KU_B_USEDSPINS + 8u + 15u) & ~15u;     // :936
        static_assert(KU_B_SPINEFFECTS == KU_VP_SPINEFFECTS_OFF, "VP: maSpinEffects[8] @0x1230");

        const unsigned KU_B_PREVWORLDVEL  = KU_B_SPINEFFECTS + 8u * 0x20u;          // :939
        static_assert(KU_B_PREVWORLDVEL == KU_VP_PREVWORLDVEL_OFF,
                      "VP: eight 0x20 SpinEffects land mPreviousWorldSpaceVelocity on the "
                      "asm-literal 0x1330 (Construct `li r6,0x1330 ; stvx128 v127,r31,r6`)");
        const unsigned KU_B_NORMLINVEL    = KU_B_PREVWORLDVEL + 0x10u;              // :942
        static_assert(KU_B_NORMLINVEL == KU_VP_NORMLINVELMAG_OFF,
                      "VP: mNormLinearVelocityMag on the asm-literal 0x1340 "
                      "(UpdateLinearVelocityMagnitude `addi r10,r3,0x1340`)");

        // :945..:967 -- the scalar/flag run. Eleven of its thirteen bytes are individually
        // role-attested in the header's map; the arithmetic below only has to reproduce the four
        // offsets that HAVE asm literals plus the 16-align at the end.
        const unsigned KU_B_HASAIR        = KU_B_NORMLINVEL + 0x10u;                // :945
        static_assert(KU_B_HASAIR == KU_VP_HASAIR_OFF, "VP: mbHasAir @0x1350");
        static_assert(KU_B_HASAIR + 2u == KU_VP_DRIFTSTATE_OFF,
                      "VP: mbHadAirLastFrame then mu8DriftState -> the asm-literal 0x1352 "
                      "(ExitDrift `stb r5,0x1352(r3)`)");
        // :948 mu8DriftState, :949 mi8NumWorldCollisions, then :950 miNumCollisions (4-aligned).
        static_assert(((KU_VP_DRIFTSTATE_OFF + 1u + 1u + 3u) & ~3u) == KU_VP_NUMCOLLISIONS_OFF,
                      "VP: mi8NumWorldCollisions then the 4-aligned miNumCollisions -> the "
                      "asm-literal 0x1354 (ApplyCarContactImpulse `lwz`+`stw 0x1354`)");
        static_assert(KU_VP_NUMCOLLISIONS_OFF + 4u == KU_VP_HANDBRAKE_OFF,
                      "VP: miNumCollisions(4) -> mbHandBrake on the asm-literal 0x1358");
        static_assert(KU_VP_HANDBRAKE_OFF + 1u == KU_VP_DEFORMACTIVE_OFF,
                      "⭐ VP: mbDeformationModelIsActive is the byte right after mbHandBrake -- the "
                      "0x1359 SetRaceCarCrashing sets to 1 beside the ResetDeformableAABB copy. "
                      "(The record used to call this `mbCrashCommitted` at 3097.)");
        static_assert(KU_VP_DEFORMACTIVE_OFF + 4u == KU_VP_JUSTBEENSLAMMED_OFF,
                      "VP: mbDeformedThisFrame/mbAllWheelsHaveTraction/mbResetCarTransform sit "
                      "between it and the asm-literal 0x135D (AddSlam `stb r10,0x135D`)");
        static_assert(KU_VP_JUSTBEENSLAMMED_OFF + 2u == KU_VP_WEDGED_OFF,
                      "VP: mbOverrideSteering between mbJustBeenSlammed and the asm-literal 0x135F "
                      "(DoPlayerStuckLineTests)");
        static_assert(KU_VP_WEDGED_OFF + 1u == KU_VP_FRONTRAY_OFF,
                      "VP: the two DoPlayerStuckLineTests bytes are adjacent");
        static_assert(KU_VP_FRONTRAY_OFF + 1u == KU_VP_BURNOUT_OFF,
                      "VP: mbDoingBurnout on the asm-literal 0x1361 (UpdateBurnout)");
        static_assert(KU_VP_BURNOUT_OFF + 1u == KU_VP_CONTACTINGWALL_OFF,
                      "VP: mbContactingWall on the asm-literal 0x1362 (CheckForGrindingAndRubbing)");

        const unsigned KU_B_PREVTRANSFORM = (KU_VP_CONTACTINGWALL_OFF + 1u + 15u) & ~15u;   // :969
        static_assert(KU_B_PREVTRANSFORM == KU_VP_PREVTRANSFORM_OFF,
                      "⭐ VP: the flag run must 16-round onto the asm-literal 0x1370 "
                      "(GetTransformDelta `addi r11,r4,0x1370` + four row loads)");

        const unsigned KU_B_LASTLINVEL    = KU_B_PREVTRANSFORM + 0x40u;   // :970
        const unsigned KU_B_PITCHYAWROLL  = KU_B_LASTLINVEL    + 0x10u;   // :972
        static_assert(KU_B_PITCHYAWROLL == KU_VP_PITCHYAWROLL_OFF,
                      "⭐ VP: Matrix44Affine(0x40) + mLastLinearVelocity must land "
                      "mPitchYawRollFromTakeOff on the asm-literal 0x13C0 (UpdateInAirBehaviour)");

        const unsigned KU_B_WHEELFFSPRING = KU_B_PITCHYAWROLL + 0x10u;    // :973
        static_assert(KU_B_WHEELFFSPRING == KU_VP_WHEELFFSPRING_OFF,
                      "VP: mWheelFFSpring on the asm-literal 0x13D0 (UpdateDriving `stfs 0x13D0` "
                      "and `stfs 0x13D4` -- TWO floats, which is exactly the DWARF struct)");
        const unsigned KU_B_ROLLINGINAIR  = KU_B_WHEELFFSPRING + 8u;      // :974
        static_assert(KU_B_ROLLINGINAIR == KU_VP_ROLLINGINAIR_OFF,
                      "⭐ VP: sizeof(WheelFFSpring)==8 lands mbRollingInAir on the asm-literal "
                      "0x13D8 (UpdateInAirBehaviour `stb`/`lbz 0x13D8`)");
        const unsigned KU_B_CARTYPE       = (KU_B_ROLLINGINAIR + 1u + 3u) & ~3u;   // :977
        static_assert(KU_B_CARTYPE == KU_VP_CARTYPE_OFF,
                      "⭐ VP: the 4-aligned meCarType on the asm-literal 0x13DC -- the seat the "
                      "record used to call `mfPlayerBoostStrengthStat`, and it is stored with "
                      "`stw` by BOTH VehiclePhysics::Prepare and VehicleManager::ApplyPlayerStats");
        const unsigned KU_B_LASTATTACKER  = KU_B_CARTYPE + 4u;            // :979
        static_assert(KU_B_LASTATTACKER == KU_VP_LASTATTACKER_OFF,
                      "VP: mi8LastAttackersRaceCarIndex on the asm-literal 0x13E0 (AddSlam/AddShunt)");
        const unsigned KU_B_DEBUGCOMPONENT = (KU_B_LASTATTACKER + 1u + 3u) & ~3u;  // :982
        static_assert(KU_B_DEBUGCOMPONENT == KU_VP_DEBUGCOMPONENT_OFF,
                      "VP: the 4-aligned console pointer mpDebugComponent on the asm-literal "
                      "0x13E4 (ApplySuspensionForces / UpdateDownForce `lwz r11,0x13E4`)");

        // ⭐⭐ THE CLOSURE. The last member is a 4-byte console pointer, so the block's last data
        // byte is 0x13E7; 16-rounding lands on 0x13F0 -- which is where a DIFFERENT wave, from a
        // DIFFERENT function, put RaceCarPhysics::mPropCollisionImpulseSum, and from which THAT
        // block closes with zero slack on the 0x1460 per-car stride a THIRD function bakes as a
        // literal. Nothing in this file knows about that derivation; it just has to land on it.
        const unsigned KU_B_VP_SIZEOF = (KU_B_DEBUGCOMPONENT + 4u + 15u) & ~15u;
        static_assert(KU_B_VP_SIZEOF == KU_VP_SIZEOF,
                      "⭐⭐ VP CLOSURE: the own block must end exactly at 0x13F0 == the start of "
                      "the RaceCarPhysics own block");
    }

    // ==============================================================================================
    // PART 2 -- the HOST-side sizes the arithmetic above depends on. These ARE checkable with
    // sizeof, because each of these sub-structs is pointer-free and is reconstructed member-for-
    // member from the DWARF, so the host reproduces the console size exactly. If any of them ever
    // drifts, PART 1's arithmetic silently stops describing what this tree actually declares --
    // which is precisely the failure mode this half exists to catch.
    // ==============================================================================================
    void VehiclePhysics::_AssertOwnBlockLayout()
    {
        static_assert(sizeof(VehiclePhysics::SlamEffect) == 0x30,
                      "SlamEffect: Vector3 + six f32 + s8 -> 0x30 (the 0x1130 - 0x1100 gap)");
        // ⭐ AddSlam @0x825D4870 addresses SIX of SlamEffect's seven fields by absolute offset off
        // the 0x1100 base, so each of these is an asm literal minus 0x1100 -- and unlike the sizeof
        // above they are NOT blind to a member added inside the struct (tamper case 6).
        static_assert(offsetof(VehiclePhysics::SlamEffect, mfSteering) == 0x14,
                      "AddSlam `stfs f0,0x1114(r11)`");
        static_assert(offsetof(VehiclePhysics::SlamEffect, mfOriginalSteering) == 0x18,
                      "AddSlam `stfs f0,0x1118(r11)` -- the SAME register as mfSteering");
        static_assert(offsetof(VehiclePhysics::SlamEffect, mfSlamLife) == 0x1C,
                      "AddSlam `lfs f0,0x111C(r11)` (the IsActive() test) and `stfs f1,0x111C`");
        static_assert(offsetof(VehiclePhysics::SlamEffect, mfTotalSlamTime) == 0x20,
                      "AddSlam `lfs f12,0x1120(r11)` / `stfs f1,0x1120`");
        static_assert(offsetof(VehiclePhysics::SlamEffect, mfRecoveryTime) == 0x24,
                      "AddSlam `stfs f3,0x1124(r11)`");
        static_assert(offsetof(VehiclePhysics::SlamEffect, mi8SlamNumber) == 0x28,
                      "AddSlam `lbz r10,0x1128(r11)` ... `stb r10,0x1128(r11)`, clamped to "
                      "KI8_MAX_NUM_SLAMS-1 == 2");
        // The above-ground result's interior is pinned the same way, by
        // SimpleVehiclePhysics::Construct's writes off `addi r11,r31,0x570`.
        static_assert(offsetof(AboveGroundTestResult, mIntersectionNormal) == 0x10,
                      "SVP::Construct `stvx128 v1,r11,0x10`");
        static_assert(offsetof(AboveGroundTestResult, mfVerticalDistance) == 0x20,
                      "SVP::Construct `stfs f0,0x20(r11)`");
        static_assert(offsetof(AboveGroundTestResult, mCollisionTag) == 0x24,
                      "SVP::Construct `sth r6,0x24(r11)` + `sth r7,0x26(r11)` -- one u32 tag, two "
                      "halfword stores");
        static_assert(offsetof(AboveGroundTestResult, mbValid) == 0x28,
                      "SVP::Construct `stb r30,0x28(r11)`");
        static_assert(sizeof(VehiclePhysics::ShuntEffect) == 0x20,
                      "ShuntEffect: Vector3Plus + Vector4 -> 0x20 (the 0x1150 - 0x1130 gap)");
        static_assert(sizeof(VehiclePhysics::AirRamEffect) == 0x30,
                      "AirRamEffect: two Vector3 + f32 + InputSpace + f32 -> 0x30 (the "
                      "0x1220 - 0x1160 gap over four entries)");
        static_assert(sizeof(VehiclePhysics::SpinEffect) == 0x20,
                      "SpinEffect: Vector3 + two f32 -> 0x20 (the 0x1330 - 0x1230 gap over eight)");
        static_assert(sizeof(CgsInput::Device::WheelFFSpring) == 8,
                      "WheelFFSpring: two f32 -- UpdateDriving writes 0x13D0 and 0x13D4 and nothing "
                      "else, and mbRollingInAir is at 0x13D8");
        static_assert(sizeof(CgsContainers::BitArray<KU_MAX_AIR_RAMS>) == 8,
                      "BitArray<4>: one 64-bit field (UpdateAirRam's `ld` walk)");
        static_assert(sizeof(CgsContainers::BitArray<KU_MAX_SPINS>) == 8,
                      "BitArray<8>: one 64-bit field (UpdateSpinEffects' `ld` walk)");
        static_assert(sizeof(BrnPlayerDriverControls) == X360Layout::KU_VP_CONTROLS_SIZE,
                      "BrnPlayerDriverControls: 0x48, the literal VehiclePhysics::UpdateDriving "
                      "memcpys -- and the reason meDriverType lands on 0x10D4");
        // meDriverType is `protected` inside BrnPlayerDriverControls, so its offset cannot be taken
        // from here; BrnVehicleDriverControls.h owns that seat and pins it at +0x44 from its own
        // six asm attestations (BrnNetworkDriverControls::Clear, UpdateDriving's memcpy 0x48, the
        // two role-confirmed bool ends of the run, UpdateBoost, and the AddEvent record size).
        // PART 1 above asserts the consequence -- 0x1090 + 0x44 == the asm-literal 0x10D4 -- which
        // is the part this wave is responsible for.
        static_assert(sizeof(BrnPhysics::SuspensionSpring) == X360Layout::KU_VP_SPRING_STRIDE,
                      "SuspensionSpring: 0x30 == the maSprings stride Construct's Prepare loop uses");
        static_assert(sizeof(AboveGroundTestResult) == X360Layout::KU_SVP_ABOVEGROUND_SIZE,
                      "AboveGroundTestResult: 0x30 == the 0x5A0 - 0x570 gap Construct pins");
        static_assert(sizeof(CgsGeometric::AxisAlignedBox) == X360Layout::KU_SVP_AABB_SIZE,
                      "AxisAlignedBox: 0x20 == the 32 bytes SetRaceCarCrashing copies 0x6F0 -> 0x6D0");

        // The two classes' own members must at least still EXIST under the names the map uses.
        // (Absolute offsets are deliberately not asserted -- see the banner.) Referencing them
        // through offsetof is the cheapest compile-time existence proof that also proves the
        // ACCESS: a member that moved to another class, or lost its name, stops compiling here.
        typedef VehiclePhysics V;
        static_assert(offsetof(V, mpAttribs) < offsetof(V, mAIVehicleAttribs), "DWARF order :822 < :823");
        static_assert(offsetof(V, mAIVehicleAttribs) < offsetof(V, mPlayerVehicleAttribs), "DWARF :823 < :824");
        static_assert(offsetof(V, mvSpringMassScalers) < offsetof(V, mWeightTransfer), "DWARF :829 < :830");
        static_assert(offsetof(V, mWeightTransfer)
                          < offsetof(V, mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare),
                      "DWARF :830 < :831");
        static_assert(offsetof(V, mUsedAirRams) < offsetof(V, mAirRamEffect), "DWARF :932 < :933");
        static_assert(offsetof(V, mAirRamEffect) < offsetof(V, mUsedSpins), "DWARF :933 < :935");
        static_assert(offsetof(V, mUsedSpins) < offsetof(V, maSpinEffects), "DWARF :935 < :936");
        static_assert(offsetof(V, mLastLinearVelocity) < offsetof(V, mPitchYawRollFromTakeOff),
                      "DWARF :970 < :972");
        static_assert(offsetof(V, mPitchYawRollFromTakeOff) < offsetof(V, mbRollingInAir),
                      "DWARF :972 < :974 (mWheelFFSpring is declared earlier in this header)");
        static_assert(offsetof(V, mbRollingInAir) < offsetof(V, meCarType), "DWARF :974 < :977");
        static_assert(offsetof(V, meCarType) < offsetof(V, mpDebugComponent), "DWARF :977 < :982");

        // ⚠️ THE MEASURED BLIND SPOT, and what covers it. Two runs of this block live entirely
        // inside alignment padding, so NO arithmetic and NO sizeof can see a member added to or
        // dropped from them (tamper cases 2, 6 and 9 of this wave -- all three silent, by
        // construction, not by oversight):
        //     * the four bools :912/:913/:916/:919 between mDriftFlags (0x10F4) and mSlamEffect
        //       (0x1100) -- 0x10F5..0x10F8 is 11 bytes of slack;
        //     * the 8 bytes after mpDebugComponent (0x13E8..0x13F0);
        //     * anything appended after the last field of a 16-aligned sub-struct.
        // What DOES cover them: the DWARF list is exhaustive and is quoted verbatim in the two
        // headers, and every one of these members is named below, so DELETING one stops this TU
        // compiling even though the arithmetic cannot count it. That is the honest boundary.
        (void)offsetof(V, mbInBoostKick);               // :912
        (void)offsetof(V, mbForceFrozen);               // :913  (UpdateFreezing `lbz r9,0x10F6`)
        (void)offsetof(V, mbIsUsingAIDonutAttribs);     // :916
        (void)offsetof(V, mbGivenAftertouchAirBoost);   // :919
        (void)offsetof(V, mbDeformedThisFrame);         // :955
        (void)offsetof(V, mbOverrideSteering);          // :960
        (void)offsetof(V, mbIsWedgedInWorld);           // :961
        (void)offsetof(V, mbIsFrontRayOccluded);        // :962
        (void)offsetof(V, mbContactingWall);            // :967

        // The members this wave ADDED that the record's seven suspect fields fold onto, named once
        // here so a rename anywhere breaks the build rather than the map.
        (void)offsetof(V, mPreviousControls);           // record +4308 lives at +0x44 inside it
        (void)offsetof(V, mbDeformationModelIsActive);  // record +3097's real seat (0x1359)
        (void)offsetof(V, meCarType);                   // record +5084's real seat (0x13DC)
        (void)offsetof(V, mvfWheelFrictionLinearMultiplier);
        (void)offsetof(V, mLastLinearVelocity);
        (void)offsetof(V, mpDebugComponent);
    }
}
}
