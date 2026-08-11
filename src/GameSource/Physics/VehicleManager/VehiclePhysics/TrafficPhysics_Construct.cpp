#include "GameSource/Physics/VehicleManager/VehiclePhysics/TrafficPhysics.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"  // BrnPlayerDriverControls (Update's 72-byte copy)

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT (Update's default arm)
#include "rw/math/vpu/vector3_operation.h"             // rw::math::vpu::Mult (the heavy-crash damping)

#include <cmath>    // std::pow (the vlogefp/vexptefp pair -> DampenAngularVelocity precedent)
#include <cstring>  // std::memcpy (the 0x48 control-block copy)

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
    namespace vpu = rw::math::vpu;
    // The four freak-out FSM constants (moved here with Update, 2026-08-03; they were in
    // TrafficPhysics.cpp and Update is their only consumer).
    // ⭐ RE-NAMED 2026-08-09 (crash/shunt wave): the gate's operand is splat(mpAttribs Mass .x)
    // -- the SAME +0x70 lane UpdateCrashing's mass regime reads -- vs flt_82019638 == 5000.0.
    // It is a heavy-vehicle MASS gate, not a speed gate.
    static const f32 KF_TRAFFIC_CONTROL_HALVE_MASS = 5000.0f;   // flt_82019638
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
    // Update  @0x82639590  ⭐⭐ RECONCILED FULL 2026-08-09 (crash/shunt wave; 457 insns read
    // line by line -- every flagged stand-in below is gone).
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
    // too. Both VehiclePhysics callees (UpdateShunt / UpdateCrashing) are now BODIED in
    // VehiclePhysics.cpp -- the VehiclePhysicsLinkStubs traps died with this wave.
    //
    // What the 2026-08-03 slice had WRONG, all settled off the asm this wave:
    //   * "speed > 5000" -- the gate reads splat(mpAttribs Mass .x) > 5000.0 [flt_82019638]: a
    //     MASS gate (heavy traffic), and it halves GAS always + BRAKE only in gear 0
    //     (`lwz 0xFC0` == mEngine.mu8CurrentGear word, reverse/neutral) -- not throttle+steer.
    //   * the control mirrors were inert locals -- the real body memcpy's the 72-byte block and
    //     the FSM WRITES the copy (mfGas/mfBrake/mfSteering/mfHandBrake), which then feeds the
    //     shared solvers.
    //   * missed entirely: mLastLinearVelocity (+0x13B0) and mPreviousTransform (+0x1370, four
    //     stvx128 rows) are snapshotted every frame (asm 0x8263964C..0x8263968C).
    //   * the FSM severity is the copy's mfHandBrake channel (the traffic driver packs its
    //     severity there), the direction handed to SetFreakedOut is the copy's mfSteering, and
    //     every case writes the wheel-friction multiplier register (+0xFD0,
    //     mvfWheelFrictionLinearMultiplier).
    //   * UpdateShunt receives the COPY + the dt splat (v1), not the caller's block.
    //   * before UpdateCrashing, a crashing HEAVY car (mass >= 3500 [unk_82FB9300 <- init thunk
    //     @0x82C5CF18]) gets an extra exponential damping pair: v *= 0.99^(60*dt) on both
    //     velocity registers (bases unk_82FB9100/unk_82FB9BD0 <- flt_820224B0 == 0.99, thunks
    //     @0x82C5CEBC/@0x82C5CEE0; the 60.0 is flt_82092BC4). std::pow per the
    //     DampenAngularVelocity precedent (the console inlines the vlogefp/vexptefp polynomial).
    //   * UpdateCrashing receives the REAL pass-through args (r5 camera, the copy, r7/r8/r9
    //     bools verbatim) -- not null/false stand-ins.
    // ---------------------------------------------------------------------------------------------
    void TrafficPhysics::Update(f32 lfTimeStep, f32 lfUnused, const Matrix44Affine* lpCameraMatrix,
                                const BrnPlayerDriverControls* lpControls, bool lbImpactTime,
                                bool lbPlayerAftertouchForceAdditive, bool lbShowtimeAllowed)
    {
        // The traffic heavy-crash damping bank (image-recovered; see the banner).
        static const f32 KF_TRAFFIC_CRASH_DAMP_MIN_MASS = 3500.0f;   // unk_82FB9300 <- 0x8205878C
        static const f32 KF_TRAFFIC_CRASH_DAMP_BASE     = 0.99f;     // unk_82FB9100/9BD0 <- 0x820224B0
        static const f32 KF_DAMP_RATE_SCALE             = 60.0f;     // flt_82092BC4
        // The TURN_AND_ROLL wheel-friction multiplier: the console lazily splats flt_82004744 ==
        // 0.2 into unk_82FBA2C0 behind the dword_82FBA2D0 bit-0 latch; the latch is a
        // static-init idiom with no observable state, so the constant lands directly.
        static const f32 KF_TURNROLL_WHEEL_FRICTION     = 0.2f;      // flt_82004744

        (void)lfUnused;   // the f2 slot is never read (its first mention is the FSM's own load)

        const VecFloat lvfTimeStep{ lfTimeStep, lfTimeStep, lfTimeStep, lfTimeStep };

        // 0x826395A8..0x826395C8: the 72-byte control block, copied so the FSM's writes do not
        // corrupt the caller's input. The COPY is what feeds UpdateShunt/UpdateCrashing.
        BrnPlayerDriverControls lCopy;
        std::memcpy(&lCopy, lpControls, sizeof(BrnPlayerDriverControls));

        // --- the heavy-vehicle control halving (asm 0x826395CC..0x82639648) ---
        {
            const f32 lfMass =
                mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;
            if (lfMass > KF_TRAFFIC_CONTROL_HALVE_MASS)          // vcmpgtfp vs 5000.0
            {
                lCopy.mfGas *= 0.5f;                             // flt_82001DA0
                if (mEngine.GetCurrentGear() == 0)               // lwz 0xFC0 == 0 (rev/neutral)
                    lCopy.mfBrake *= 0.5f;
            }
        }

        // --- the per-frame previous-state snapshots (asm 0x8263964C..0x8263968C) ---
        mLastLinearVelocity = mLinearVelocity;                   // stvx -> +0x13B0
        mPreviousTransform  = mTransform;                        // 4x stvx -> +0x1370..+0x13A0

        // --- the freak-out FSM (mu8FreakOutState @ +0x13F4; asm 0x82639690..0x826398B8) ---
        // The LCG-draw archaeology (the divide-by-101 magic reciprocal, the RandomUInt inline)
        // is unchanged from the 2026-08-03 correction -- see git history; the draw is
        // `RandomUInt() % 101 >= 50`.
        f32 lfNewFreakOutTime = 0.0f;
        switch (mu8FreakOutState)
        {
        case E_FREAK_OUT_STATE_OFF:
        {
            // Entry (asm 0x826397F8..0x82639898): the severity channel is the COPY's
            // mfHandBrake (+0xC -- the traffic driver packs its severity there); the direction
            // handed to SetFreakedOut is the COPY's mfSteering (var_80 == copy+0x10).
            if (lCopy.mfHandBrake > KF_FREAKOUT_SEVERITY_GATE)   // fcmpu vs 0.5
            {
                const u32 luDraw = mRandom.RandomUInt();
                SetFreakedOut(lCopy.mfSteering,
                              (luDraw % 101u) >= 50u ? KF_FREAKOUT_SEVERITY_HIGH_DRAW
                                                     : KF_FREAKOUT_SEVERITY_LOW_DRAW);
                if (!IsCrashing())                               // lbz +0x710 == 0
                    SetCrashing();                               // vcall +0x08
            }
            // Both legs converge on loc_8263989C: full wheel friction + a ZEROED timer.
            mvfWheelFrictionLinearMultiplier = VecFloat{ 1.0f, 1.0f, 1.0f, 1.0f };   // +0xFD0
            lfNewFreakOutTime = 0.0f;                            // flt_82001CC0
            break;
        }
        case E_FREAK_OUT_STATE_INITIAL:
            // INITIAL -> SPIN_OUT immediately; if not already crashing, fire the crash virtual.
            mu8FreakOutState = E_FREAK_OUT_STATE_SPIN_OUT;       // stb 3 -> +0x13F4
            mvfWheelFrictionLinearMultiplier = VecFloat{ 1.0f, 1.0f, 1.0f, 1.0f };
            if (!IsCrashing())
                SetCrashing();
            lfNewFreakOutTime = mfFreakOutTime + lfTimeStep;
            break;

        case E_FREAK_OUT_STATE_TURN_AND_ROLL:
            // asm 0x8263972C..0x826397B0: steer to the stored freak-out direction, handbrake
            // hard on, wheel friction down to 0.2; full gas only inside the 2.0 s window.
            lCopy.mfSteering  = mfFreakOutDirection;             // stfs +0x13F8 -> copy+0x10
            lCopy.mfHandBrake = 1.0f;                            // flt_82001C98 -> copy+0xC
            mvfWheelFrictionLinearMultiplier =
                VecFloat{ KF_TURNROLL_WHEEL_FRICTION, KF_TURNROLL_WHEEL_FRICTION,
                          KF_TURNROLL_WHEEL_FRICTION, KF_TURNROLL_WHEEL_FRICTION };
            if (mfFreakOutTime < KF_FREAKOUT_TURNROLL_MAX)       // fcmpu vs 2.0 [flt_82001D9C]
                lCopy.mfGas = 1.0f;
            lfNewFreakOutTime = mfFreakOutTime + lfTimeStep;
            break;

        case E_FREAK_OUT_STATE_SPIN_OUT:
            // asm 0x826396E0..0x82639728: times out at 4.0 s; steer to the freak-out direction
            // with gas AND brake pinned to 1 (not "throttle+steer" as the old slice had it).
            if (mfFreakOutTime > KF_FREAKOUT_SPINOUT_TIMEOUT)    // fcmpu vs 4.0 [flt_8208FA0C]
                mu8FreakOutState = E_FREAK_OUT_STATE_OFF;
            lCopy.mfSteering = mfFreakOutDirection;
            lCopy.mfBrake    = 1.0f;
            lCopy.mfGas      = 1.0f;
            mvfWheelFrictionLinearMultiplier = VecFloat{ 1.0f, 1.0f, 1.0f, 1.0f };
            lfNewFreakOutTime = mfFreakOutTime + lfTimeStep;
            break;

        default:
            CGS_ASSERT(false, "false");   // the X360 asserts on an out-of-range state
            lfNewFreakOutTime = mfFreakOutTime + lfTimeStep;
            break;
        }
        mfFreakOutTime = lfNewFreakOutTime;   // stfs -> +0x13FC (the common tail)

        // The shared shunt solver -- the SAME entry the player uses -- fed the COPY and the dt
        // splat (asm 0x826398B4..0x826398CC: r4 = &copy, v1 = dt).
        VehiclePhysics::UpdateShunt(&lCopy, lvfTimeStep);

        // --- the heavy-traffic crash damping (asm 0x826398D0..0x82639C84) ---
        // While crashing AND heavy (mass >= 3500), both velocity registers decay by
        // 0.99^(60*dt) -- the inlined vlogefp/vexptefp pair, landed as std::pow over the
        // image-recovered constants (see the banner).
        if (IsCrashing())
        {
            const f32 lfMass =
                mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;
            if (lfMass >= KF_TRAFFIC_CRASH_DAMP_MIN_MASS)        // vcmpgefp
            {
                const f32 lfFactor =
                    std::pow(KF_TRAFFIC_CRASH_DAMP_BASE, KF_DAMP_RATE_SCALE * lfTimeStep);
                mLinearVelocity  = vpu::Mult(mLinearVelocity,  lfFactor);   // stvx +0x50
                mAngularVelocity = vpu::Mult(mAngularVelocity, lfFactor);   // stvx +0x60
            }
        }

        // The shared crash solver with the REAL pass-through arguments (asm 0x82639C88..
        // 0x82639CA4: r5 = incoming camera, r6 = &copy, r7/r8/r9 = the incoming bools).
        VehiclePhysics::UpdateCrashing(lfTimeStep, lpCameraMatrix, &lCopy, lbImpactTime,
                                       lbPlayerAftertouchForceAdditive, lbShowtimeAllowed);
    }
}
}
