// Native Xbox 360 vehicle layouts. The actual C++ member offsets are checked
// against ARTIST, with DecFIGS supplying declaration order and names.
// The older anchor arithmetic remains as an additional check; it cannot prove
// that a C++ object uses that layout, so it is followed by real offsetof gates.

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
        CGS_STATIC_ASSERT((KU_A_ABOVEGROUND == KU_SVP_ABOVEGROUND_OFF),
                      "SVP: the wheel/sphere/traction arrays must land mAboveGroundTestResult on the "
                      "asm-literal 0x570 (Construct `addi r11,r31,0x570`)");

        const unsigned KU_A_SIMPLEATTRIBS = KU_A_ABOVEGROUND + KU_SVP_ABOVEGROUND_SIZE;    // :361
        CGS_STATIC_ASSERT((KU_A_SIMPLEATTRIBS == KU_SVP_SIMPLEATTRIBS_OFF),
                      "SVP: AboveGroundTestResult (pos,normal,f32,tag,bool -> 0x30) must land "
                      "mSimpleAttribs on the asm-literal 0x5A0 (`addi r3,r31,0x5A0`)");

        // sizeof(SimpleVehicleAttribs) on the console is NOT independently attested -- it is the gap
        // between two asm literals, which is exactly how it is written here, so this line is a
        // definition and not a check. It is the ONE console size in this block that is not
        // separately witnessed; flagged rather than dressed up.
        const unsigned KU_SVP_SIMPLEATTRIBS_SIZE =
            KU_SVP_HANDLINGBODYOFFSET - KU_SVP_SIMPLEATTRIBS_OFF;                          // 240
        CGS_STATIC_ASSERT((KU_SVP_SIMPLEATTRIBS_SIZE == 240u), "SVP: 0x690 - 0x5A0 == 240");

        const unsigned KU_A_HANDLINGBODY  = KU_A_SIMPLEATTRIBS + KU_SVP_SIMPLEATTRIBS_SIZE; // :362
        const unsigned KU_A_HALFEXTENT    = KU_A_HANDLINGBODY + 0x10u;                      // :363
        const unsigned KU_A_WHEELPLANE    = KU_A_HALFEXTENT   + 0x10u;                      // :364
        const unsigned KU_A_SPEEDMPH      = KU_A_WHEELPLANE   + 0x10u;                      // :365
        const unsigned KU_A_DEFORMAABB    = KU_A_SPEEDMPH     + 0x10u;                      // :366
        CGS_STATIC_ASSERT((KU_A_HALFEXTENT == 0x6A0u),
                      "SVP: mHalfExtent on the asm-literal 0x6A0 (Construct `li r8,0x6A0`)");
        CGS_STATIC_ASSERT((KU_A_SPEEDMPH == 0x6C0u),
                      "SVP: mfSpeedMPH on the asm-literal 0x6C0 (UpdateHandBrake `li r7,0x6C0`)");
        CGS_STATIC_ASSERT((KU_A_DEFORMAABB == KU_SVP_DEFORMABLE_AABB_OFF),
                      "SVP: mDeformableAABB on the asm-literal 0x6D0 (SetRaceCarCrashing's "
                      "ResetDeformableAABB copy DESTINATION)");

        const unsigned KU_A_ORIGINALAABB  = KU_A_DEFORMAABB   + KU_SVP_AABB_SIZE;           // :367
        CGS_STATIC_ASSERT((KU_A_ORIGINALAABB == KU_SVP_ORIGINAL_AABB_OFF),
                      "SVP: mOriginalAABB on the asm-literal 0x6F0 (the same copy's SOURCE)");

        const unsigned KU_A_MBCRASHING    = KU_A_ORIGINALAABB + KU_SVP_AABB_SIZE;           // :368
        CGS_STATIC_ASSERT((KU_A_MBCRASHING == KU_SVP_MBCRASHING_OFF),
                      "SVP: mbCrashing on 0x710 -- the seat the console's own assert string names "
                      "(FireAssert(\"mbCrashing\", \".../RaceCarPhysics.h\", 328))");

        // :369..:373 are five more bools -> last data byte 0x715, then 16-align.
        const unsigned KU_A_SVP_RAW_END   = KU_A_MBCRASHING + 6u;                           // 0x716
        const unsigned KU_A_SVP_SIZEOF    = (KU_A_SVP_RAW_END + 15u) & ~15u;
        CGS_STATIC_ASSERT((KU_A_SVP_SIZEOF == KU_SVP_SIZEOF),
                      "⭐ SVP CLOSURE: the six trailing bools must round the block to 0x720");

        // ---------- VehiclePhysics: 0x720 -> 0x13F0 ----------
        // DWARF VehiclePhysics.h:822-982, in declaration order.
        CGS_STATIC_ASSERT((KU_SVP_SIZEOF == KU_VP_MPATTRIBS_OFF),
                      "⭐ THE FIRST JOIN: sizeof(SimpleVehiclePhysics) must equal the asm-literal "
                      "offset VehiclePhysics::Construct stores mpAttribs at (`stw r30,0x720(r31)`). "
                      "Two functions, no shared assumption.");

        // mpAttribs (4 bytes on the console) + the 12 bytes of alignment the 16-aligned
        // VehicleAttribs forces.
        CGS_STATIC_ASSERT((KU_VP_AIATTRIBS_OFF == ((KU_VP_MPATTRIBS_OFF + 4u + 15u) & ~15u)),
                      "VP: mAIVehicleAttribs must be the next 16-aligned slot after the 4-byte "
                      "mpAttribs (asm: `addi r3,r31,0x730`)");
        CGS_STATIC_ASSERT((KU_VP_PLAYERATTRIBS_OFF == KU_VP_AIATTRIBS_OFF + KU_VP_VEHICLEATTRIBS_SIZE),
                      "VP: the two VehicleAttribs Construct calls are 0x370 apart");

        const unsigned KU_B_SPRINGS       = KU_VP_PLAYERATTRIBS_OFF + KU_VP_VEHICLEATTRIBS_SIZE;
        CGS_STATIC_ASSERT((KU_B_SPRINGS == KU_VP_SPRINGS_OFF),
                      "⭐ VP: mPlayerVehicleAttribs + 0x370 must land maSprings on the asm-literal "
                      "0xE10 (Construct `addi r29,r31,0xE10`). Three asm literals, one arithmetic.");

        const unsigned KU_B_SPRINGSCALERS = KU_B_SPRINGS + 4u * KU_VP_SPRING_STRIDE;
        CGS_STATIC_ASSERT((KU_B_SPRINGSCALERS == KU_VP_SPRINGMASSSCALERS_OFF),
                      "VP: 4 springs at the asm-literal 0x30 stride must land mvSpringMassScalers "
                      "on the asm-literal 0xED0 (Construct `li r11,0xED0`)");

        const unsigned KU_B_WEIGHTXFER    = KU_B_SPRINGSCALERS + 0x10u;   // :830
        const unsigned KU_B_SPEEDONCRASH  = KU_B_WEIGHTXFER    + 0x10u;   // :831
        CGS_STATIC_ASSERT((KU_B_SPEEDONCRASH == KU_VP_SPEEDONLASTCRASH_OFF),
                      "VP: mWeightTransfer between them puts mvSpeedOnLastCrashMPH_... on the "
                      "asm-literal 0xEF0 (Construct `li r28,0xEF0`)");

        const unsigned KU_B_ENGINE        = KU_B_SPEEDONCRASH + 0x10u;    // :833
        CGS_STATIC_ASSERT((KU_B_ENGINE == KU_VP_ENGINE_OFF),
                      "VP: mEngine on the asm-literal 0xF00 (Construct `addi r30,r31,0xF00`)");

        // sizeof(Engine): mAttribs is the asm-literal 0xA0 (Engine::Prepare `memcpy(this,src,0xA0)`),
        // then two Vector4s put mu8CurrentGear at the asm-literal Engine+0xC0 (Prepare
        // `stw r10,0xC0(r31)`), then two bools -> 0xC6 -> 16-round -> 0xD0.
        CGS_STATIC_ASSERT((KU_VP_ENGINEATTRIBS_SIZE + 2u * 0x10u == KU_VP_ENGINE_GEAR_OFF),
                      "Engine: EngineAttribs(0xA0) + two Vector4s must land mu8CurrentGear on the "
                      "asm-literal Engine+0xC0");
        CGS_STATIC_ASSERT((((KU_VP_ENGINE_GEAR_OFF + 4u + 2u + 15u) & ~15u) == KU_VP_ENGINE_SIZE),
                      "Engine: mu8CurrentGear(u32) + two bools -> sizeof(Engine) == 0xD0");

        const unsigned KU_B_WHEELFRICTION = KU_B_ENGINE + KU_VP_ENGINE_SIZE;   // :835
        const unsigned KU_B_STEERINGANGLE = KU_B_WHEELFRICTION + 0x10u;        // :879
        CGS_STATIC_ASSERT((KU_B_STEERINGANGLE == KU_VP_STEERINGANGLE_OFF),
                      "⭐ VP: sizeof(Engine)==0xD0 plus mvfWheelFrictionLinearMultiplier must land "
                      "mvSteeringAngle_... on the asm-literal 0xFE0 (GetSteeringAngle "
                      "`addi r11,r31,0xFE0`, whose lane .x IS the steering angle)");

        // :879..:889 is ELEVEN Vector4s (mvSteeringAngle_... is :879 and is already placed), so ten
        // more steps of 0x10 reach the last of them. Together with the VecFloat at 0xFD0 the run is
        // twelve 16-byte registers, 0xFD0..0x1090.
        const unsigned KU_B_HANDBRAKETIMERS = KU_B_STEERINGANGLE + 10u * 0x10u;   // :889
        CGS_STATIC_ASSERT((KU_B_HANDBRAKETIMERS == KU_VP_HANDBRAKETIMERS_OFF),
                      "⭐ VP: the eleven-Vector4 bank must end on the asm-literal 0x1080 -- the "
                      "register UpdateHandBrake works lanes z/w of, which the DWARF member name "
                      "calls TimeHandbrakeHasBeenOn / TimeSinceLastHandBrake");

        const unsigned KU_B_PREVCONTROLS  = KU_B_HANDBRAKETIMERS + 0x10u;         // :905
        CGS_STATIC_ASSERT((KU_B_PREVCONTROLS + 0x44u == KU_VP_DRIVERTYPE_OFF),
                      "⭐ VP: mPreviousControls at 0x1090 puts its meDriverType (+0x44, a seat an "
                      "EARLIER wave pinned from BrnNetworkDriverControls::Clear) on the asm-literal "
                      "0x10D4 that VehiclePhysics::Prepare stores with `stw r30,0x10D4(r31)`");

        const unsigned KU_B_STEERINGDIR   = (KU_B_PREVCONTROLS + KU_VP_CONTROLS_SIZE + 15u) & ~15u;  // :906
        CGS_STATIC_ASSERT((KU_B_STEERINGDIR == 0x10E0u), "VP: mSteeringDirection @0x10E0");
        const unsigned KU_B_STUCKTIMER    = KU_B_STEERINGDIR + 0x10u;   // :907
        CGS_STATIC_ASSERT((KU_B_STUCKTIMER == KU_VP_STUCKTIMER_OFF),
                      "VP: mfTimeUntilStuckInCollisionTest on the asm-literal 0x10F0 "
                      "(UpdatePlayerStuckInCollisionSpheres `lfs`/`stfs 0x10F0`)");
        const unsigned KU_B_DRIFTFLAGS    = KU_B_STUCKTIMER + 4u;       // :910
        CGS_STATIC_ASSERT((KU_B_DRIFTFLAGS == KU_VP_DRIFTFLAGS_OFF),
                      "VP: mDriftFlags on the asm-literal 0x10F4 (ExitDrift `stb r4,0x10F4(r3)`)");

        // :912/:913/:916/:919 -- four bools -> 0x10F9 -> 16-round.
        const unsigned KU_B_SLAMEFFECT    = (KU_B_DRIFTFLAGS + 1u + 4u + 15u) & ~15u;   // :927
        CGS_STATIC_ASSERT((KU_B_SLAMEFFECT == KU_VP_SLAMEFFECT_OFF),
                      "⭐ VP: mDriftFlags + four bools must land mSlamEffect on 0x1100 -- the base "
                      "AddSlam addresses ALL SEVEN SlamEffect fields off, at the sub-struct's own "
                      "DWARF offsets (+0x14/+0x18 steering pair, +0x1C life, +0x20 total, +0x24 "
                      "recovery, +0x28 the slam number it clamps to KI8_MAX_NUM_SLAMS-1)");

        const unsigned KU_B_SHUNTEFFECT   = KU_B_SLAMEFFECT + 0x30u;   // :928
        CGS_STATIC_ASSERT((KU_B_SHUNTEFFECT == KU_VP_SHUNTEFFECT_OFF),
                      "VP: sizeof(SlamEffect)==0x30 lands mShuntEffect on the asm-literal 0x1130");
        const unsigned KU_B_LASTCONTACTED = KU_B_SHUNTEFFECT + 0x20u;  // :929
        CGS_STATIC_ASSERT((KU_B_LASTCONTACTED == KU_VP_LASTCONTACTED_OFF),
                      "VP: sizeof(ShuntEffect)==0x20 lands mi8LastContactedRaceCar on the "
                      "asm-literal 0x1150 (HandleRaceCarRaceCarContact `stb r14,0x1150(r16)`)");

        // BitArray<N> is one 64-bit field, so it is 8-ALIGNED: the s8 above leaves 7 bytes of pad.
        const unsigned KU_B_USEDAIRRAMS   = (KU_B_LASTCONTACTED + 1u + 7u) & ~7u;   // :932
        CGS_STATIC_ASSERT((KU_B_USEDAIRRAMS == KU_VP_USEDAIRRAMS_OFF),
                      "⭐ VP: mUsedAirRams on the asm-literal 0x1158 -- both the address Construct "
                      "zeroes with `std r30,0x1158(r31)` and the base UpdateAirRam walks with a "
                      "`ld`/`cntlzd` scan BOUNDED AT 4, which is the template argument");
        const unsigned KU_B_AIRRAMEFFECT  = (KU_B_USEDAIRRAMS + 8u + 15u) & ~15u;   // :933
        CGS_STATIC_ASSERT((KU_B_AIRRAMEFFECT == KU_VP_AIRRAMEFFECT_OFF),
                      "VP: mAirRamEffect[4] @0x1160");
        const unsigned KU_B_USEDSPINS     = KU_B_AIRRAMEFFECT + 4u * 0x30u;         // :935
        CGS_STATIC_ASSERT((KU_B_USEDSPINS == KU_VP_USEDSPINS_OFF),
                      "⭐ VP: four 0x30 AirRamEffects land mUsedSpins on the asm-literal 0x1220 -- "
                      "Construct's `std r30,0x1220(r31)` and UpdateSpinEffects' scan BOUNDED AT 8");
        const unsigned KU_B_SPINEFFECTS   = (KU_B_USEDSPINS + 8u + 15u) & ~15u;     // :936
        CGS_STATIC_ASSERT((KU_B_SPINEFFECTS == KU_VP_SPINEFFECTS_OFF), "VP: maSpinEffects[8] @0x1230");

        const unsigned KU_B_PREVWORLDVEL  = KU_B_SPINEFFECTS + 8u * 0x20u;          // :939
        CGS_STATIC_ASSERT((KU_B_PREVWORLDVEL == KU_VP_PREVWORLDVEL_OFF),
                      "VP: eight 0x20 SpinEffects land mPreviousWorldSpaceVelocity on the "
                      "asm-literal 0x1330 (Construct `li r6,0x1330 ; stvx128 v127,r31,r6`)");
        const unsigned KU_B_NORMLINVEL    = KU_B_PREVWORLDVEL + 0x10u;              // :942
        CGS_STATIC_ASSERT((KU_B_NORMLINVEL == KU_VP_NORMLINVELMAG_OFF),
                      "VP: mNormLinearVelocityMag on the asm-literal 0x1340 "
                      "(UpdateLinearVelocityMagnitude `addi r10,r3,0x1340`)");

        // :945..:967 -- the scalar/flag run. Eleven of its thirteen bytes are individually
        // role-attested in the header's map; the arithmetic below only has to reproduce the four
        // offsets that HAVE asm literals plus the 16-align at the end.
        const unsigned KU_B_HASAIR        = KU_B_NORMLINVEL + 0x10u;                // :945
        CGS_STATIC_ASSERT((KU_B_HASAIR == KU_VP_HASAIR_OFF), "VP: mbHasAir @0x1350");
        CGS_STATIC_ASSERT((KU_B_HASAIR + 2u == KU_VP_DRIFTSTATE_OFF),
                      "VP: mbHadAirLastFrame then mu8DriftState -> the asm-literal 0x1352 "
                      "(ExitDrift `stb r5,0x1352(r3)`)");
        // :948 mu8DriftState, :949 mi8NumWorldCollisions, then :950 miNumCollisions (4-aligned).
        CGS_STATIC_ASSERT((((KU_VP_DRIFTSTATE_OFF + 1u + 1u + 3u) & ~3u) == KU_VP_NUMCOLLISIONS_OFF),
                      "VP: mi8NumWorldCollisions then the 4-aligned miNumCollisions -> the "
                      "asm-literal 0x1354 (ApplyCarContactImpulse `lwz`+`stw 0x1354`)");
        CGS_STATIC_ASSERT((KU_VP_NUMCOLLISIONS_OFF + 4u == KU_VP_HANDBRAKE_OFF),
                      "VP: miNumCollisions(4) -> mbHandBrake on the asm-literal 0x1358");
        CGS_STATIC_ASSERT((KU_VP_HANDBRAKE_OFF + 1u == KU_VP_DEFORMACTIVE_OFF),
                      "⭐ VP: mbDeformationModelIsActive is the byte right after mbHandBrake -- the "
                      "0x1359 SetRaceCarCrashing sets to 1 beside the ResetDeformableAABB copy. "
                      "(The record used to call this `mbCrashCommitted` at 3097.)");
        CGS_STATIC_ASSERT((KU_VP_DEFORMACTIVE_OFF + 4u == KU_VP_JUSTBEENSLAMMED_OFF),
                      "VP: mbDeformedThisFrame/mbAllWheelsHaveTraction/mbResetCarTransform sit "
                      "between it and the asm-literal 0x135D (AddSlam `stb r10,0x135D`)");
        CGS_STATIC_ASSERT((KU_VP_JUSTBEENSLAMMED_OFF + 2u == KU_VP_WEDGED_OFF),
                      "VP: mbOverrideSteering between mbJustBeenSlammed and the asm-literal 0x135F "
                      "(DoPlayerStuckLineTests)");
        CGS_STATIC_ASSERT((KU_VP_WEDGED_OFF + 1u == KU_VP_FRONTRAY_OFF),
                      "VP: the two DoPlayerStuckLineTests bytes are adjacent");
        CGS_STATIC_ASSERT((KU_VP_FRONTRAY_OFF + 1u == KU_VP_BURNOUT_OFF),
                      "VP: mbDoingBurnout on the asm-literal 0x1361 (UpdateBurnout)");
        CGS_STATIC_ASSERT((KU_VP_BURNOUT_OFF + 1u == KU_VP_CONTACTINGWALL_OFF),
                      "VP: mbContactingWall on the asm-literal 0x1362 (CheckForGrindingAndRubbing)");

        const unsigned KU_B_PREVTRANSFORM = (KU_VP_CONTACTINGWALL_OFF + 1u + 15u) & ~15u;   // :969
        CGS_STATIC_ASSERT((KU_B_PREVTRANSFORM == KU_VP_PREVTRANSFORM_OFF),
                      "⭐ VP: the flag run must 16-round onto the asm-literal 0x1370 "
                      "(GetTransformDelta `addi r11,r4,0x1370` + four row loads)");

        const unsigned KU_B_LASTLINVEL    = KU_B_PREVTRANSFORM + 0x40u;   // :970
        const unsigned KU_B_PITCHYAWROLL  = KU_B_LASTLINVEL    + 0x10u;   // :972
        CGS_STATIC_ASSERT((KU_B_PITCHYAWROLL == KU_VP_PITCHYAWROLL_OFF),
                      "⭐ VP: Matrix44Affine(0x40) + mLastLinearVelocity must land "
                      "mPitchYawRollFromTakeOff on the asm-literal 0x13C0 (UpdateInAirBehaviour)");

        const unsigned KU_B_WHEELFFSPRING = KU_B_PITCHYAWROLL + 0x10u;    // :973
        CGS_STATIC_ASSERT((KU_B_WHEELFFSPRING == KU_VP_WHEELFFSPRING_OFF),
                      "VP: mWheelFFSpring on the asm-literal 0x13D0 (UpdateDriving `stfs 0x13D0` "
                      "and `stfs 0x13D4` -- TWO floats, which is exactly the DWARF struct)");
        const unsigned KU_B_ROLLINGINAIR  = KU_B_WHEELFFSPRING + 8u;      // :974
        CGS_STATIC_ASSERT((KU_B_ROLLINGINAIR == KU_VP_ROLLINGINAIR_OFF),
                      "⭐ VP: sizeof(WheelFFSpring)==8 lands mbRollingInAir on the asm-literal "
                      "0x13D8 (UpdateInAirBehaviour `stb`/`lbz 0x13D8`)");
        const unsigned KU_B_CARTYPE       = (KU_B_ROLLINGINAIR + 1u + 3u) & ~3u;   // :977
        CGS_STATIC_ASSERT((KU_B_CARTYPE == KU_VP_CARTYPE_OFF),
                      "⭐ VP: the 4-aligned meCarType on the asm-literal 0x13DC -- the seat the "
                      "record used to call `mfPlayerBoostStrengthStat`, and it is stored with "
                      "`stw` by BOTH VehiclePhysics::Prepare and VehicleManager::ApplyPlayerStats");
        const unsigned KU_B_LASTATTACKER  = KU_B_CARTYPE + 4u;            // :979
        CGS_STATIC_ASSERT((KU_B_LASTATTACKER == KU_VP_LASTATTACKER_OFF),
                      "VP: mi8LastAttackersRaceCarIndex on the asm-literal 0x13E0 (AddSlam/AddShunt)");
        const unsigned KU_B_DEBUGCOMPONENT = (KU_B_LASTATTACKER + 1u + 3u) & ~3u;  // :982
        CGS_STATIC_ASSERT((KU_B_DEBUGCOMPONENT == KU_VP_DEBUGCOMPONENT_OFF),
                      "VP: the 4-aligned console pointer mpDebugComponent on the asm-literal "
                      "0x13E4 (ApplySuspensionForces / UpdateDownForce `lwz r11,0x13E4`)");

        // THE CLOSURE. The last member is a 4-byte console pointer, so the block's last data
        // byte is 0x13E7; 16-rounding lands on 0x13F0 -- which is where a DIFFERENT wave, from a
        // DIFFERENT function, put RaceCarPhysics::mPropCollisionImpulseSum, and from which THAT
        // block closes with zero slack on the 0x1460 per-car stride a THIRD function bakes as a
        // literal. Nothing in this file knows about that derivation; it just has to land on it.
        const unsigned KU_B_VP_SIZEOF = (KU_B_DEBUGCOMPONENT + 4u + 15u) & ~15u;
        CGS_STATIC_ASSERT((KU_B_VP_SIZEOF == KU_VP_SIZEOF),
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
        // ARTIST native object layout, including the individual byte flags.
        CGS_STATIC_ASSERT((sizeof(VehiclePhysics) == 0x13F0), "X360 VehiclePhysics size");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mpAttribs) == 0x720), "X360 mpAttribs");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mAIVehicleAttribs) == 0x730), "X360 mAIVehicleAttribs");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mPlayerVehicleAttribs) == 0xAA0), "X360 mPlayerVehicleAttribs");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, maSprings) == 0xE10), "X360 maSprings");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mvSpringMassScalers) == 0xED0), "X360 mvSpringMassScalers");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mWeightTransfer) == 0xEE0), "X360 mWeightTransfer");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare) == 0xEF0), "X360 mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mEngine) == 0xF00), "X360 mEngine");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mvfWheelFrictionLinearMultiplier) == 0xFD0), "X360 mvfWheelFrictionLinearMultiplier");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount) == 0xFE0), "X360 mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mvPlayerStatSpeed_PlayerStatStrength_PlayerStatControl_PlayerStatBoost) == 0xFF0), "X360 mvPlayerStatSpeed_PlayerStatStrength_PlayerStatControl_PlayerStatBoost");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale) == 0x1000), "X360 mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale) == 0x1010), "X360 mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState) == 0x1020), "X360 mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle) == 0x1030), "X360 mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime) == 0x1040), "X360 mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor) == 0x1050), "X360 mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction) == 0x1060), "X360 mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact) == 0x1070), "X360 mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake) == 0x1080), "X360 mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mPreviousControls) == 0x1090), "X360 mPreviousControls");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mSteeringDirection) == 0x10E0), "X360 mSteeringDirection");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mfTimeUntilStuckInCollisionTest) == 0x10F0), "X360 mfTimeUntilStuckInCollisionTest");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mDriftFlags) == 0x10F4), "X360 mDriftFlags");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbInBoostKick) == 0x10F5), "X360 mbInBoostKick");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbForceFrozen) == 0x10F6), "X360 mbForceFrozen");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbIsUsingAIDonutAttribs) == 0x10F7), "X360 mbIsUsingAIDonutAttribs");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbGivenAftertouchAirBoost) == 0x10F8), "X360 mbGivenAftertouchAirBoost");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mSlamEffect) == 0x1100), "X360 mSlamEffect");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mShuntEffect) == 0x1130), "X360 mShuntEffect");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mi8LastContactedRaceCar) == 0x1150), "X360 mi8LastContactedRaceCar");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mUsedAirRams) == 0x1158), "X360 mUsedAirRams");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mAirRamEffect) == 0x1160), "X360 mAirRamEffect");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mUsedSpins) == 0x1220), "X360 mUsedSpins");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, maSpinEffects) == 0x1230), "X360 maSpinEffects");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mPreviousWorldSpaceVelocity) == 0x1330), "X360 mPreviousWorldSpaceVelocity");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mNormLinearVelocityMag) == 0x1340), "X360 mNormLinearVelocityMag");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbHasAir) == 0x1350), "X360 mbHasAir");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbHadAirLastFrame) == 0x1351), "X360 mbHadAirLastFrame");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mu8DriftState) == 0x1352), "X360 mu8DriftState");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mi8NumWorldCollisions) == 0x1353), "X360 mi8NumWorldCollisions");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, miNumCollisions) == 0x1354), "X360 miNumCollisions");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbHandBrake) == 0x1358), "X360 mbHandBrake");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbDeformationModelIsActive) == 0x1359), "X360 mbDeformationModelIsActive");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbDeformedThisFrame) == 0x135A), "X360 mbDeformedThisFrame");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbAllWheelsHaveTraction) == 0x135B), "X360 mbAllWheelsHaveTraction");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbResetCarTransform) == 0x135C), "X360 mbResetCarTransform");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbJustBeenSlammed) == 0x135D), "X360 mbJustBeenSlammed");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbOverrideSteering) == 0x135E), "X360 mbOverrideSteering");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbIsWedgedInWorld) == 0x135F), "X360 mbIsWedgedInWorld");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbIsFrontRayOccluded) == 0x1360), "X360 mbIsFrontRayOccluded");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbDoingBurnout) == 0x1361), "X360 mbDoingBurnout");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbContactingWall) == 0x1362), "X360 mbContactingWall");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mPreviousTransform) == 0x1370), "X360 mPreviousTransform");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mLastLinearVelocity) == 0x13B0), "X360 mLastLinearVelocity");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mPitchYawRollFromTakeOff) == 0x13C0), "X360 mPitchYawRollFromTakeOff");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mWheelFFSpring) == 0x13D0), "X360 mWheelFFSpring");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mbRollingInAir) == 0x13D8), "X360 mbRollingInAir");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, meCarType) == 0x13DC), "X360 meCarType");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mi8LastAttackersRaceCarIndex) == 0x13E0), "X360 mi8LastAttackersRaceCarIndex");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, mpDebugComponent) == 0x13E4), "X360 mpDebugComponent");

        // ==========================================================================================
        // ⭐ THE SimpleVehiclePhysics TAIL, PINNED ON THE HOST (added 2026-09-03, traffic
        // crash-severity wave).
        //
        // These five were previously NOT gated -- BrnSimpleVehiclePhysics.h's own-member banner
        // claimed "the absolute console offsets below are NOT reproducible as host offsetofs".
        // MEASURED with offsetof on this tree's x64 build, every one of them IS: 1728/1744/1776/
        // 1808/1824 == 0x6C0/0x6D0/0x6F0/0x710/0x720, identical to the console literals. This
        // matters because DeformableObject::UpdateDeformedBBox (BrnDeformableObject_BBox.cpp) is
        // the ONLY producer of mDeformableAABB and every consumer -- IsFrontCornerClip,
        // PredictCarCarIntersection, the UpdateSkinningOffsets clamp (97c3a7e1), the [T5-ram]
        // witness -- reads it BY NAME. A header edit that shifted the tail would silently divorce
        // producer from consumers; now it cannot compile.
        //
        // This function is a member of VehiclePhysics, which derives from SimpleVehiclePhysics.
        // ⚠️ The offsetof must name the DERIVED class -- `offsetof(SimpleVehiclePhysics, m)` is an
        // access error even here (the naming class decides access, and these members are protected in
        // the base); `offsetof(VehiclePhysics, m)` is the same number and is allowed, because single
        // non-virtual inheritance puts the base sub-object at 0.
        //
        // ⚠️ The console literals live in X360Layout (BrnSimpleVehiclePhysics.h) so the two halves
        // of each line are the two INDEPENDENT derivations, not one number typed twice: the
        // right-hand side is the X360 asm literal, the left is what MSVC actually laid out.
        // ==========================================================================================

        CGS_STATIC_ASSERT((sizeof(VehiclePhysics::SlamEffect) == 0x30),
                      "SlamEffect: Vector3 + six f32 + s8 -> 0x30 (the 0x1130 - 0x1100 gap)");
        // AddSlam @0x825D4870 addresses SIX of SlamEffect's seven fields by absolute offset off
        // the 0x1100 base, so each of these is an asm literal minus 0x1100 -- and unlike the sizeof
        // above they are NOT blind to a member added inside the struct (tamper case 6).
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics::SlamEffect, mfSteering) == 0x14),
                      "AddSlam `stfs f0,0x1114(r11)`");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics::SlamEffect, mfOriginalSteering) == 0x18),
                      "AddSlam `stfs f0,0x1118(r11)` -- the SAME register as mfSteering");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics::SlamEffect, mfSlamLife) == 0x1C),
                      "AddSlam `lfs f0,0x111C(r11)` (the IsActive() test) and `stfs f1,0x111C`");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics::SlamEffect, mfTotalSlamTime) == 0x20),
                      "AddSlam `lfs f12,0x1120(r11)` / `stfs f1,0x1120`");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics::SlamEffect, mfRecoveryTime) == 0x24),
                      "AddSlam `stfs f3,0x1124(r11)`");
        CGS_STATIC_ASSERT((offsetof(VehiclePhysics::SlamEffect, mi8SlamNumber) == 0x28),
                      "AddSlam `lbz r10,0x1128(r11)` ... `stb r10,0x1128(r11)`, clamped to "
                      "KI8_MAX_NUM_SLAMS-1 == 2");
        // The above-ground result's interior is pinned the same way, by
        // SimpleVehiclePhysics::Construct's writes off `addi r11,r31,0x570`.
        CGS_STATIC_ASSERT((offsetof(AboveGroundTestResult, mIntersectionNormal) == 0x10),
                      "SVP::Construct `stvx128 v1,r11,0x10`");
        CGS_STATIC_ASSERT((offsetof(AboveGroundTestResult, mfVerticalDistance) == 0x20),
                      "SVP::Construct `stfs f0,0x20(r11)`");
        CGS_STATIC_ASSERT((offsetof(AboveGroundTestResult, mCollisionTag) == 0x24),
                      "SVP::Construct `sth r6,0x24(r11)` + `sth r7,0x26(r11)` -- one u32 tag, two "
                      "halfword stores");
        CGS_STATIC_ASSERT((offsetof(AboveGroundTestResult, mbValid) == 0x28),
                      "SVP::Construct `stb r30,0x28(r11)`");
        CGS_STATIC_ASSERT((sizeof(VehiclePhysics::ShuntEffect) == 0x20),
                      "ShuntEffect: Vector3Plus + Vector4 -> 0x20 (the 0x1150 - 0x1130 gap)");
        CGS_STATIC_ASSERT((sizeof(VehiclePhysics::AirRamEffect) == 0x30),
                      "AirRamEffect: two Vector3 + f32 + InputSpace + f32 -> 0x30 (the "
                      "0x1220 - 0x1160 gap over four entries)");
        CGS_STATIC_ASSERT((sizeof(VehiclePhysics::SpinEffect) == 0x20),
                      "SpinEffect: Vector3 + two f32 -> 0x20 (the 0x1330 - 0x1230 gap over eight)");
        CGS_STATIC_ASSERT((sizeof(CgsInput::Device::WheelFFSpring) == 8),
                      "WheelFFSpring: two f32 -- UpdateDriving writes 0x13D0 and 0x13D4 and nothing "
                      "else, and mbRollingInAir is at 0x13D8");
        CGS_STATIC_ASSERT((sizeof(BrnPlayerDriverControls) == X360Layout::KU_VP_CONTROLS_SIZE),
                      "BrnPlayerDriverControls: 0x48, the literal VehiclePhysics::UpdateDriving "
                      "memcpys -- and the reason meDriverType lands on 0x10D4");
        // meDriverType is `protected` inside BrnPlayerDriverControls, so its offset cannot be taken
        // from here; BrnVehicleDriverControls.h owns that seat and pins it at +0x44 from its own
        // six asm attestations (BrnNetworkDriverControls::Clear, UpdateDriving's memcpy 0x48, the
        // two role-confirmed bool ends of the run, UpdateBoost, and the AddEvent record size).
        // PART 1 above asserts the consequence -- 0x1090 + 0x44 == the asm-literal 0x10D4 -- which
        // is the part this wave is responsible for.
        CGS_STATIC_ASSERT((sizeof(BrnPhysics::SuspensionSpring) == X360Layout::KU_VP_SPRING_STRIDE),
                      "SuspensionSpring: 0x30 == the maSprings stride Construct's Prepare loop uses");
        CGS_STATIC_ASSERT((sizeof(AboveGroundTestResult) == X360Layout::KU_SVP_ABOVEGROUND_SIZE),
                      "AboveGroundTestResult: 0x30 == the 0x5A0 - 0x570 gap Construct pins");
        CGS_STATIC_ASSERT((sizeof(CgsGeometric::AxisAlignedBox) == X360Layout::KU_SVP_AABB_SIZE),
                      "AxisAlignedBox: 0x20 == the 32 bytes SetRaceCarCrashing copies 0x6F0 -> 0x6D0");

        // The two classes' own members must at least still EXIST under the names the map uses.
        // (The native absolute offsets are asserted above.) Referencing them
        // through offsetof is the cheapest compile-time existence proof that also proves the
        // ACCESS: a member that moved to another class, or lost its name, stops compiling here.
        typedef VehiclePhysics V;
        CGS_STATIC_ASSERT((offsetof(V, mpAttribs) < offsetof(V, mAIVehicleAttribs)), "DWARF order :822 < :823");
        CGS_STATIC_ASSERT((offsetof(V, mAIVehicleAttribs) < offsetof(V, mPlayerVehicleAttribs)), "DWARF :823 < :824");
        CGS_STATIC_ASSERT((offsetof(V, mvSpringMassScalers) < offsetof(V, mWeightTransfer)), "DWARF :829 < :830");
        CGS_STATIC_ASSERT((offsetof(V, mWeightTransfer)
                          < offsetof(V, mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare)),
                      "DWARF :830 < :831");
        CGS_STATIC_ASSERT((offsetof(V, mUsedAirRams) < offsetof(V, mAirRamEffect)), "DWARF :932 < :933");
        CGS_STATIC_ASSERT((offsetof(V, mAirRamEffect) < offsetof(V, mUsedSpins)), "DWARF :933 < :935");
        CGS_STATIC_ASSERT((offsetof(V, mUsedSpins) < offsetof(V, maSpinEffects)), "DWARF :935 < :936");
        CGS_STATIC_ASSERT((offsetof(V, mLastLinearVelocity) < offsetof(V, mPitchYawRollFromTakeOff)),
                      "DWARF :970 < :972");
        CGS_STATIC_ASSERT((offsetof(V, mPitchYawRollFromTakeOff) < offsetof(V, mbRollingInAir)),
                      "DWARF :972 < :974 (mWheelFFSpring is declared earlier in this header)");
        CGS_STATIC_ASSERT((offsetof(V, mbRollingInAir) < offsetof(V, meCarType)), "DWARF :974 < :977");
        CGS_STATIC_ASSERT((offsetof(V, meCarType) < offsetof(V, mpDebugComponent)), "DWARF :977 < :982");

        // THE MEASURED BLIND SPOT, and what covers it. Two runs of this block live entirely
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
