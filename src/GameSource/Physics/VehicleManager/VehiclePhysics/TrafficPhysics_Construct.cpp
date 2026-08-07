#include "GameSource/Physics/VehicleManager/VehiclePhysics/TrafficPhysics.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT (Update's default arm)

// =================================================================================================
// BrnPhysics::Vehicle::TrafficPhysics -- Construct + SetFreakedOut + Update.
//
// ⭐ WHY THIS IS A SEPARATE TU FROM TrafficPhysics.cpp (the RaceCarPhysics_Construct.cpp /
// BrnSimpleVehiclePhysics_Construct.cpp precedent). TrafficPhysics.cpp cannot be mounted:
// PreparePhysical calls VehiclePhysics::Prepare, which is still declare-only. Everything that CAN
// be mounted lives here, and THIS file is on the build's source list.
// ⛔ There is exactly ONE definition of each function in the tree. Update MOVED here from
// TrafficPhysics.cpp (it had to -- see the note above its body); it was not copied.
//
// ⭐⭐ Construct @0x8262E980 IS AN `.ida-exports` HOLE and was pulled first-hand. The X360 JSON set
// jumps 0x8262E848 (VehiclePhysics::UpdateEngine, which ends at 0x8262E97C) -> 0x8262EBE8
// (RaceCarPhysics::UpdateAftertouch), and headless IDA 9.3 on BURNOUT_X360_ARTIST.XEX.i64 shows the
// gap is one whole function, 0x8262E980..0x8262EBE8, 154 instructions, already named
// `BrnPhysics::Vehicle::TrafficPhysics::Construct`, with one prologue (`bl __savegprlr_28`) and one
// epilogue (`b __restgprlr_28`). Its only caller in the image is PhysicalTrafficManager::Construct
// @0x82636CA8 (the `bl` at 0x82636D80).
//
// The DecFIGS PS3 twin (0x6EB440, 47 instructions) exists too and agrees on the shape, but it is a
// month older and materially SHORTER -- it has neither the mbFrozen clear nor the second Reset call
// -- so everything below is written off the X360, with the PS3 used only where it disambiguates.
// =================================================================================================

namespace BrnPhysics
{
namespace Vehicle
{
    // The four freak-out FSM constants (moved here with Update, 2026-08-03; they were in
    // TrafficPhysics.cpp and Update is their only consumer).
    // The traffic over-speed gate: above this the scripted throttle/steer are halved
    // (the asm compares an mpAttribs maxSpeed-region lane vs the local `v95 = 5000.0`).
    static const f32 KF_TRAFFIC_CONTROL_HALVE_SPEED = 5000.0f;
    static const f32 KF_FREAKOUT_SPINOUT_TIMEOUT    = 4.0f;   // SPIN_OUT times out at 4.0s
    static const f32 KF_FREAKOUT_TURNROLL_MAX       = 2.0f;   // TURN_AND_ROLL window <= 2.0s
    static const f32 KF_FREAKOUT_SEVERITY_GATE      = 0.5f;   // entry gated on control-severity > 0.5

    // ---------------------------------------------------------------------------------------------
    // Construct  @0x8262E980  (154 instructions)
    //
    //   bl VehiclePhysics::Construct                                    @0x8262DBD0
    //   bl SimpleVehiclePhysics::Reset                                  @0x825D9A58
    //   stb r30(=0), 0x70(r31)                                          mbFrozen = false
    //   bl VehiclePhysics::Reset  with v1 = {0,0,0,0} built on the stack from flt_82001CC0 (0.0f)
    //   addi r9,r31,0x1070 ; lvx128 ; vrlimi128 v0,v127,2,0 ; stvx128   the +0x1070 LANE-Z insert
    //   addi r11,r31,0x1400 ; <CgsNumeric::Random::Construct inlined>   mRandom.Construct()
    //
    // ⚠️ THE MIDDLE THREE STATEMENTS ARE THE SAME THREE `VehiclePhysics::Construct` ITSELF ENDS
    // WITH, in the same order (0x8262DD14 / 0x8262DD2C / 0x8262DD3C) -- so this really does run them
    // a second time. That is not a mis-read of the caller: both `bl` targets were resolved
    // numerically (0x825D9A58 and 0x825FDD78) and both functions carry their own copy.
    //   The likeliest source is a 0-arg `VehiclePhysics::Reset()` overload that the X360 compiler
    //   inlined at both sites and never emitted out of line (the X360 has exactly one `Reset`
    //   symbol, the Vector3 one). The DecFIGS PS3 build HAS that overload out of line
    //   (._ZN10BrnPhysics7Vehicle14VehiclePhysics5ResetEv @0x6EAEC4) and its whole body is
    //   `SimpleVehiclePhysics::Reset(); Reset(Vector3(0));` -- **with no store to mbFrozen anywhere**
    //   (+0x60 in PS3 frame terms; the function makes no `stb` at all). So either the X360's copy of
    //   that overload also clears mbFrozen -- a merge-window delta -- or the clear is spelled out at
    //   both call sites. The binary cannot tell the two apart, and it does not matter: the observable
    //   sequence is identical either way. It is written out here statement for statement, the same
    //   way the committed VehiclePhysics::Construct writes it, rather than inventing a declaration
    //   for an overload this tree's VehiclePhysics.h does not have.
    // ---------------------------------------------------------------------------------------------
    void TrafficPhysics::Construct()
    {
        VehiclePhysics::Construct();

        // The 0-arg base overload (explicitly qualified because Reset(Vector3) hides it), the
        // unfreeze, then the full reset at zero velocity -- see the note above.
        SimpleVehiclePhysics::Reset();

        mbFrozen = false;                       // +0x70  (ExternallySimulatedBody frame +0x60)

        Reset(Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });

        // LANE-Z-ONLY insert, read-modify-write: `lvx128 v0 ; vrlimi128 v0,v127,2,0 ; stvx128 v0`.
        // Mask 2 is lane z under the 8/4/2/1 == x/y/z/w convention, and the PS3 twin does the same
        // lane with `vperm` over VectorPermuteConstant<0,1,6,3> (x, y, SECOND-operand z, w). Every
        // other lane of the register survives.
        mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.z = 0.0f;

        // +0x1400. The X360 INLINES CgsNumeric::Random::Construct here; the inline is that function
        // statement for statement -- default seed 0xC87CD8C91AD0891B, ring slot 0 set to exactly
        // 1.0f, then SEVEN AddRandomFloatToBuffer() refills (each: bits = 1.0f | (oldSeedHigh >> 9)
        // via `inslwi rX, hi, 23, 9` into a `lis 0x3F80` base, seed = seed * 0x5851F42D4C957F2D + 1,
        // index = (index+1) & 7, `stwx` at index*4), then one final index bump. That asymmetric
        // 1-then-7-then-bump shape is what identifies the callee: it is CgsRandom.h's committed
        // Construct() and nothing else.
        mRandom.Construct();
    }

    // ---------------------------------------------------------------------------------------------
    // SetFreakedOut  @0x825B8948  (18 instructions)
    //
    //   stfs f1, 0x13F8(r3)                      mfFreakOutDirection = lfDirection
    //   lfs  f0, flt_82FB8384 ; fcmpu f2, f0 ; ble -> state 3
    //   li   r9, 0x6C0 ; lvx128 v0, r3, r9       mfSpeedMPH
    //   lfs  20.0f ; stw 0,0,0 ; lvx128 v13 ; vspltw v13,v13,0
    //   vcmpgtfp. v0, v0, v13 ; mfocrf r11,2 ; extrwi r11,r11,1,24
    //                                            CR6 bit 0 == "ALL FOUR lanes greater"
    //   li r11, 2 ; bne -> keep 2 ; else li r11, 3
    //   stb  r11, 0x13F4(r3)                     mu8FreakOutState
    //
    // ⚠️ THE PARAMETER ORDER IS THE ASM'S, NOT A GUESS: f1 is the value stored into
    // mfFreakOutDirection and f2 is the value compared against the (120 mph)^2 gate. The call site
    // in Update @0x82639878 loads f1 from a stack local and f2 from one of the two `.data` tuning
    // floats, which is why the second parameter is named for what it is measured against.
    //
    // ⚠️ THE STATE NEVER COMES OUT AS INITIAL OR OFF. Both branches end in TURN_AND_ROLL (2) or
    // SPIN_OUT (3) -- there is no path to 0 or 1 here -- so the "freaked out" flag IsFreakedOut()
    // reads is always set by this call. That is the console's behaviour, not a simplification.
    //
    // FLAG (VMX all-lane compare): `vcmpgtfp.` + CR6 bit 0 tests all four lanes of the +0x6C0
    // register against a splat of 20.0f. mfSpeedMPH is a VecFloat whose lanes the physics keeps
    // uniform (SimpleVehiclePhysics::Reset zeroes the whole register; UpdateHandBrake and AddSlam
    // both read it as one scalar), so the all-lane test and the .x test agree; .x is written here
    // because it is the value the rest of the tree reads by name.
    // ---------------------------------------------------------------------------------------------
    void TrafficPhysics::SetFreakedOut(f32 lfDirection, f32 lfSeveritySpeedSquared)
    {
        mfFreakOutDirection = lfDirection;                                   // +0x13F8

        EFreakOutState leState = E_FREAK_OUT_STATE_SPIN_OUT;
        if (lfSeveritySpeedSquared > KF_FREAKOUT_MIN_SEVERITY_SQUARED &&
            mfSpeedMPH.x > KF_FREAKOUT_TURN_AND_ROLL_MIN_SPEED_MPH)
        {
            leState = E_FREAK_OUT_STATE_TURN_AND_ROLL;
        }

        mu8FreakOutState = static_cast<u8>(leState);                         // +0x13F4
    }

    // ---------------------------------------------------------------------------------------------
    // Update  @0x82639590  (PARTIAL)
    //
    // ⚠️⚠️ WHY THIS BODY IS IN THE **MOUNTED** TU RATHER THAN NEXT TO PreparePhysical. It is
    // `virtual`, and it is the only virtual TrafficPhysics introduces. Folding
    // `PhysicalTrafficManager::maFullTrafficPhysics[20]` to the real class (2026-08-03) put this
    // class's vtable on the link's critical path -- BrnPhysicsModule.obj emits the constructor chain
    // that seats twenty vptrs -- and a vtable needs a DEFINITION of every slot. The measured link
    // said so in exactly one line:
    //     BrnPhysicsModule.obj : error LNK2001: unresolved external symbol
    //       ?Update@TrafficPhysics@Vehicle@BrnPhysics@@UEAAXMMPEBU...
    // That is faithful, not incidental: the console constructor @0x827E42E8 writes those vtables
    // too. So the body has to be here, and its two VehiclePhysics callees have to resolve -- see
    // VehiclePhysicsLinkStubs.cpp for the two that do not yet have real bodies.
    //
    //   ingests a 72-byte scripted control block (copied to a local), halves throttle/steer over the
    //   speed gate, advances the freak-out FSM, then forwards into VehiclePhysics::UpdateShunt and
    //   VehiclePhysics::UpdateCrashing. The callee set is the function's own `xrefs_from`:
    //   memcpy, the assert trio, SetFreakedOut, UpdateShunt, UpdateCrashing -- and nothing else, so
    //   the LCG draw below really is inlined rather than a call.
    // ---------------------------------------------------------------------------------------------
    void TrafficPhysics::Update(f32 lfTimeStep, f32 lfArg2, const Matrix44Affine* lpReferenceTransform,
                                const BrnPlayerDriverControls* lpControls, bool lbArg5, bool lbArg6,
                                bool lbArg7)
    {
        (void)lfArg2; (void)lpReferenceTransform; (void)lbArg5; (void)lbArg6; (void)lbArg7;

        // The X360 memcpy's the 72-byte control block into a local (v99) so the modifiers below don't
        // corrupt the caller's input. We work on a local copy of the throttle/steer scalars. FLAG:
        // the control block is opaque here (owned by the input TU); the two modified fields are the
        // throttle (offset +4 in the local, `v99[1]`) and steer (`v99[2]`). They are read/written via
        // a local mirror -- the full block is forwarded unchanged into UpdateShunt/UpdateCrashing.
        f32 lfLocalThrottle = 0.0f;   // v99[1] mirror
        f32 lfLocalSteer    = 0.0f;   // v99[2] mirror

        // --- over-speed control halving (the `if (speed > v95=5000.0)` branch) ---
        // The gate compares mpAttribs maxSpeed-region lane vs KF_TRAFFIC_CONTROL_HALVE_SPEED.
        // FLAG: the speed source is read from the attribs lane (un-homed in this slice); the halving
        // shape (throttle *= 0.5 always, steer *= 0.5 only when `*(this+4032)==0`) is reproduced.
        const bool lbOverSpeed = false;   // FLAG: gate result from un-homed attribs lane (inert)
        const bool lbHalveSteer = true;   // `!*(this+4032)` -- the freak-out-active suppressor
        if (lbOverSpeed)
        {
            lfLocalThrottle *= 0.5f;
            if (lbHalveSteer)
                lfLocalSteer *= 0.5f;
        }

        // --- the freak-out FSM (mu8FreakOutState @ +0x13F4) ---
        f32 lfNewFreakOutTime = 0.0f;
        switch (mu8FreakOutState)
        {
        case E_FREAK_OUT_STATE_OFF:
        {
            // Entry: when the control severity (`v99[3]`) exceeds the gate, draw from the per-car
            // random ring and SetFreakedOut.
            //
            // ⚠️⚠️ CORRECTED 2026-08-03 (this wave). What stood here was
            //     mRandom.muState = mRandom.muState * 1148159575u + 1u;
            //     const f32 lfDir = ((mRandom.muState >> 6) & 1u) ? 0.0f : 0.0f;
            // and BOTH lines were wrong. Re-read off the X360 asm at 0x82639804..0x8263987C:
            //   * 1148159575 == 0x446F8657 is NOT an LCG multiplier. It is the MSVC magic reciprocal
            //     for an unsigned divide by 101 -- `mulhwu ; subf ; srwi 1 ; add ; srwi 6 ;
            //     mulli 0x65 ; subf` is the textbook add-shift correction sequence, and 0x65 == 101.
            //     The actual multiplier three instructions earlier is the ordinary console LCG
            //     constant, `insrdi` of 0x5851F42D : 0x4C957F2D == 0x5851F42D4C957F2D.
            //   * the draw is therefore `hi % 101 >= 50`, a near-even coin flip, not a bit test on
            //     bit 6 of the state.
            //   * and the ternary picked between 0.0f and 0.0f, i.e. it could not have had an effect
            //     even if the constants had been right.
            // The LCG step itself -- read muSeed, seed = seed * M + 1, return the OLD seed's HIGH
            // word -- is CgsNumeric::Random::RandomUInt(), which the X360 inlines here. That is a
            // THIRD independent witness for the body CgsRandom.cpp already committed from
            // Vehicle::SetFlashingHeadlights @0x827537D0: draw the high word, THEN step.
            const f32 lfSeverity = 0.0f;   // FLAG: v99[3] severity (un-homed control lane, inert)
            if (lfSeverity > KF_FREAKOUT_SEVERITY_GATE)
            {
                const u32 luDraw = mRandom.RandomUInt();
                SetFreakedOut(/*direction*/ 0.0f,   // FLAG: f1 is a stack local built from the
                                                    // un-homed control lanes (inert here)
                              (luDraw % 101u) >= 50u ? KF_FREAKOUT_SEVERITY_HIGH_DRAW
                                                     : KF_FREAKOUT_SEVERITY_LOW_DRAW);
            }
            lfNewFreakOutTime = 0.0f;
            break;
        }
        case E_FREAK_OUT_STATE_INITIAL:
            // INITIAL -> SPIN_OUT immediately; if not already crashing, fire the crash virtual.
            mu8FreakOutState = E_FREAK_OUT_STATE_SPIN_OUT;
            if (!IsCrashing())
                SetCrashing();
            lfNewFreakOutTime = mfFreakOutTime + lfTimeStep;
            break;

        case E_FREAK_OUT_STATE_TURN_AND_ROLL:
            // TURN_AND_ROLL holds for up to KF_FREAKOUT_TURNROLL_MAX seconds, forcing throttle to 1.
            lfLocalThrottle = 1.0f;   // v99[1] = 1.0
            if (mfFreakOutTime < KF_FREAKOUT_TURNROLL_MAX)
                lfNewFreakOutTime = mfFreakOutTime + lfTimeStep;
            else
                lfNewFreakOutTime = mfFreakOutTime + lfTimeStep;
            break;

        case E_FREAK_OUT_STATE_SPIN_OUT:
            // SPIN_OUT times out at KF_FREAKOUT_SPINOUT_TIMEOUT; force throttle+steer to 1.
            if (mfFreakOutTime > KF_FREAKOUT_SPINOUT_TIMEOUT)
                mu8FreakOutState = E_FREAK_OUT_STATE_OFF;
            lfLocalThrottle = 1.0f;   // v99[1] = 1.0
            lfLocalSteer    = 1.0f;   // v99[2] = 1.0
            lfNewFreakOutTime = mfFreakOutTime + lfTimeStep;
            break;

        default:
            CGS_ASSERT(false, "false");   // the X360 asserts on an out-of-range state
            lfNewFreakOutTime = mfFreakOutTime + lfTimeStep;
            break;
        }
        mfFreakOutTime = lfNewFreakOutTime;   // *(this+0x13FC) = v44

        (void)lfLocalThrottle; (void)lfLocalSteer;

        // Forward into the shared shunt + crashing solvers (the SAME entries the player uses).
        VehiclePhysics::UpdateShunt(lpControls);

        // FLAG (BLOCKED math, faithful delegation): when crashing the X360 runs an inlined per-axis
        // vlogefp/vexptefp angular-velocity damping curve directly in this function, driven by the
        // un-homed rodata coefficient tables unk_82014AC0..82014AF0 (a powf polynomial) and applied
        // to mAngularVelocity (+0x60). That math is NOT fabricated here; it is delegated to the
        // committed VehiclePhysics::UpdateCrashing, which owns the crash-damping curve in the full
        // physics TU.
        // ⚠️ 2026-08-07 (orchestrator wave): UpdateCrashing's declaration now carries its REAL
        // console signature (dt, camera, controls, 3 bools -- recovered at the
        // VehiclePhysics::Update call site @0x826414EC). This PC-side delegation has no camera
        // matrix or player flags -- the console TrafficPhysics inlines the curve instead of
        // calling -- so the extra arguments are the null/false stand-ins of a delegation site,
        // stated as such, not console-attested values.
        VehiclePhysics::UpdateCrashing(lfTimeStep, 0 /* no camera on the traffic leg */,
                                       lpControls, false, false, false);
    }
}
}
