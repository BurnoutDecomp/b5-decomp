#include "GameSource/Physics/VehicleManager/VehiclePhysics/Engine.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint ([powertrain] probe)

#include <algorithm> // std::min, std::max
#include <cmath>     // std::fabs
#include <cstddef>   // offsetof
#include <cstdlib>   // getenv ([powertrain] probe)
#include <cstring>   // std::memcpy

// ⭐ The present counter, so a [powertrain] line NAMES THE DUMPED FRAME it belongs to: the frame
// dump writes bb_<present>.bmp, so `present 7380` is inside bb_007380.bmp's period. Without it a
// log number and a picture can only be aligned by wall-clock guesswork, which is exactly how a
// "108 mph crash" turned out to be a 16.6 m/s yard pile-up. Same extern the [deform-bbox] witness
// takes (BrnDeformableObject_BBox.cpp:19).
namespace renderengine { extern u32 guPresentCount; }

// BrnPhysics::Vehicle::Engine -- the two ledger functions owned by the Vehicle-physics group.
// The X360 build is VMX128 inline asm; these are the de-SIMD'd named-member equivalents
// recovered store-for-store from the asm at 0x825F3EE8 (Construct) and 0x825F3F38 (Prepare).

namespace BrnPhysics
{
namespace Vehicle
{
    // ---- THE ASSERT SET (declared private in Engine.h; see the comment there) -----------------
    // Every number is an address literal in Reset @0x825CF130 / Prepare @0x825F3F38.
    void Engine::BpAssertConsoleLayout()
    {
        static_assert(offsetof(Engine, mvEngineDrive_ReactionTorque_FlyWheelAngularVelocity_ClutchDelay)
                      == 0xA0, "Reset writes EngineDrive/ReactionTorque/FlyWheel/ClutchDelay at [this+0xA0]");
        static_assert(offsetof(Engine, mvClutchFactor_RPM_CurrentGearChangeTime)
                      == 0xB0, "Reset writes ClutchFactor/RPM/CurrentGearChangeTime at [this+0xB0]");
        static_assert(offsetof(Engine, mu8CurrentGear)
                      == 0xC0, "Reset: stw <gear>, 0xC0(this); Prepare: stw <1>, 0xC0(this)");
        static_assert(offsetof(Engine, mbAllowToChangeUpGear)
                      == 0xC4, "Reset: stb <1>, 0xC4(this)");
        static_assert(offsetof(Engine, mbAllowToChangeDownGear)
                      == 0xC5, "Reset: stb <1>, 0xC5(this)");

        // The gear array is addressed by the console as `16 * (gear + 4) + this` -- the `+4` IS
        // the 0x40 base of mAttribs.mavGearRatios, and mAttribs is Engine's leading member. If
        // either of those two facts moved, Reset and ComputeGear would index the wrong register.
        static_assert(offsetof(Engine, mAttribs) == 0,
                      "mAttribs is Engine's leading member (Prepare memcpy's to `this`)");
        static_assert(offsetof(Engine, mAttribs)
                      + offsetof(EngineAttribs, mavGearRatios_TorqueScales_GearUpRPMs)
                      == 16 * 4, "gear[g] must sit at 16*(g+4) from `this`");
        static_assert(sizeof(EngineAttribs) == 0xA0, "Prepare's memcpy size is the literal 160");
    }

namespace
{
    // RECOVERED 2026-08-03. Two `.data` slots that read ALL ZEROS in the image and are filled by
    // IDA-unmarked static initialisers -- so a literal scan finds only readers, and
    // scratch/GVM/init_map_table.txt gets both of them wrong (it reports NO source for the first
    // and two contradictory sources, "2" and "1000", for the second). Both were recovered by
    // disassembling the initialiser thunks themselves (headless IDA 9.3, xrefs to the slot):
    //
    //   0x82C5B0C8:  lvlx v0,[flt_82F2A3E0] ; vspltw v0,v0,0 ; stvx128 v0 -> unk_82FB9110
    //                ⇒ unk_82FB9110 == flt_82F2A3E0 == 9.54929638 == 60 / (2*pi)
    //
    //   0x82C5C050:  lfs f0,[flt_82009E10]=1000.0 ; lfs f13,[flt_82F2A3E0] ; fdivs f0,f0,f13
    //                ; vspltw ; stvx128 -> unk_82FB9B10
    //                ⇒ unk_82FB9B10 == 1000 / 9.54929638 == 104.719757 == 1000 RPM in rad/s
    //
    // The second is a COMPUTED initialiser -- the case the init-map tool explicitly cannot see.
    // Both land on exact physical identities, which is the role check: the first is the
    // rad/s -> RPM conversion, the second is an idle flywheel speed of 1000 RPM.
    //
    // (flt_82F2A3E0 itself lives in `.data` too, but it IS initialised in the image -- raw
    // 41 18 C9 EB -- and has no writer among its five xrefs. `.data` does not imply "reads zero".)

    // unk_82FB9110 -- rad/s -> RPM. Used by ComputeGear, Reset and Engine::Update.
    const f32 KF_RADIANS_PER_SEC_TO_RPM = 9.54929638f;

    // unk_82FB9B10 -- the flywheel speed Reset seeds: 1000 RPM expressed in rad/s.
    const f32 KF_IDLE_FLYWHEEL_ANGULAR_VELOCITY = 104.719757f;

    // flt_82004A20 (.rdata) -- the "time since the last gear change" counter Reset parks high so
    // that a shift is immediately permitted (it is stored next to the two allow-change flags).
    const f32 KF_RESET_CURRENT_GEAR_CHANGE_TIME = 10.0f;

    // flt_8201FDB8 (.rdata, raw BC23D70A) -- exactly -0.01f.
    const f32 KF_REVERSE_DRIVE_THRESHOLD = -0.0099999998f;

    // ---- Engine::Update @0x825CB288 constants (powertrain wave, 2026-08-09) -------------------
    // Names are the DWARF's (DecFIGS Engine.cpp:43-161 file-scope constants). Every value below
    // was read out of the X360 image with the validated id1 reader: either directly from .rdata
    // (flt_*) or by decoding the static-initialiser bank at 0x82C5AAF0/0x82C5Bxxx..0x82C5C33x
    // that fills the `.data` splat slots (unk_82FB8xxx/9xxx) the vector leg loads. Each constant
    // lists BOTH homes (scalar-leg rdata float + vector-leg splat slot). The BPR twin sub_BA63A0
    // uses the same values by role (its .rdata is packed/unvalued in the export DB, so BPR
    // corroborates ROLE, X360 supplies the VALUE).

    // unk_82FB8B20 <- fdivs 1.0/flt_82F2A3E0 (scalar slot 0x82FB9148) -- RPM -> rad/s.
    const f32 KF_RPM_TO_RADIANS_PER_SEC = 0.104719755f;

    // stru_8208F620.x / unk_82FB80B0 -- FLT_EPSILON-class compare epsilon (2^-23).
    const f32 KF_UPDATE_EPSILON = 1.1920929e-07f;

    // flt_8208F834 / unk_82FB80C0 -- CgsNumeric::GetVecFloat_Quarter (clutch re-engage fraction).
    const f32 KF_QUARTER = 0.25f;

    // flt_82004A20 / unk_82FB8420 -- KVF_MAX_CURRENT_GEAR_CHANGE_TIME. Same rdata float Reset
    // parks the timer at (KF_RESET_CURRENT_GEAR_CHANGE_TIME above).
    const f32 KVF_MAX_CURRENT_GEAR_CHANGE_TIME = 10.0f;

    // flt_82004014 / unk_82FB8AA0 -- KVF_UPDATEENGINE_STOP_GAS (gas below this while idling).
    const f32 KVF_UPDATEENGINE_STOP_GAS = 0.1f;

    // flt_82004FDC / unk_82FB83D0 -- KVF_UPDATEENGINE_BRAKING_THRESHOLD (brake above this with
    // zero gas cuts the clutch).
    const f32 KVF_UPDATEENGINE_BRAKING_THRESHOLD = 0.95f;

    // flt_8200426C / unk_82FB90B0 -- KVF_UPDATEENGINE_HANDBRAKE_MAX_SPEED (m/s).
    const f32 KVF_UPDATEENGINE_HANDBRAKE_MAX_SPEED = 5.0f;

    // flt_82004C88 / unk_82FB83E0 -- KVF_LOW_SPEED_FIRST_GEAR_LOCK_THRESHOLD (m/s).
    const f32 KVF_LOW_SPEED_FIRST_GEAR_LOCK_THRESHOLD = 8.0f;

    // unk_82FB92E0 -- KVF_RECIP_LOW_SPEED_FIRST_GEAR_LOCK_THRESHOLD: the init thunk computes it
    // as vrefp+2xNR of the 8.0 slot (== exactly 0.125f); the branchy console leg recomputes the
    // same refined reciprocal inline.
    const f32 KVF_RECIP_LOW_SPEED_FIRST_GEAR_LOCK_THRESHOLD = 0.125f;

    // flt_82004744 / unk_82FB8860 -- KVF_STEERING_LOCK_THRESHOLD.
    const f32 KVF_STEERING_LOCK_THRESHOLD = 0.2f;

    // flt_82004014 / unk_82FB9130 -- KVF_BRAKE_LOCK_THRESHOLD (distinct slot, same 0.1 float).
    const f32 KVF_BRAKE_LOCK_THRESHOLD = 0.1f;

    // flt_82005450 / unk_82FB8B70 -- KVF_BURN_CLUTCH_IF_NOT_GOING_FORWARDS.
    const f32 KVF_BURN_CLUTCH_IF_NOT_GOING_FORWARDS = 0.9f;

    // flt_82094774 / unk_82FB9340 -- KVF_REVERSING_POWER_THROUGH_THESHOLD [sic, DWARF spelling]:
    // rolling forwards faster than this (m/s) while in reverse with the brake pressed pops the
    // box back into first.
    const f32 KVF_REVERSING_POWER_THROUGH_THESHOLD = -5.0f;

    // flt_82004D00 / unk_82FB8410 -- KVF_START_REVERSING_BRAKE_THRESHOLD.
    const f32 KVF_START_REVERSING_BRAKE_THRESHOLD = 0.6f;

    // flt_82004014 / unk_82FB8400 -- KVF_START_REVERSING_GASBRAKE_THRESHOLD (gas*brake product).
    const f32 KVF_START_REVERSING_GASBRAKE_THRESHOLD = 0.1f;

    // flt_820138DC / unk_82FB8B90 -- KF_DOWNSHIFT_FACTOR: the RPM hysteresis margin subtracted
    // (scaled by the lower gear's ratio) from the computed gear-down RPM.
    const f32 KF_DOWNSHIFT_FACTOR = 50.0f;

    // rw::math Sgn(): +1 / 0 / -1 by strict sign, exactly the console's two-vsel chain
    // (`x > 0 ? 1 : 0`, then `x >= 0 ? that : -1`).
    inline f32 BpSgn(f32 lfValue)
    {
        if (lfValue > 0.0f)  return 1.0f;
        if (lfValue >= 0.0f) return 0.0f;
        return -1.0f;
    }
}
    // ---------------------------------------------------------------------------------------
    // Construct  @0x825F3EE8
    //   asm: bl EngineAttribs::Construct(this)   -- mAttribs is the leading member (&mAttribs ==
    //        this), so this constructs the default attribs block in place; then it splats a 0.0
    //        constant into a register (the wheel-angular-velocity argument) and tail-calls
    //        Reset(0.0). No running-state writes happen here -- Reset does them.
    // ---------------------------------------------------------------------------------------
    void Engine::Construct()
    {
        mAttribs.Construct();
        Reset(VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f });
    }

    // ---------------------------------------------------------------------------------------
    // Prepare  @0x825F3F38
    //   asm: memcpy(this, lpAttribs, 160)        -- copy the supplied attribs block into mAttribs
    //        (sizeof EngineAttribs == 0xA0 == 160).
    //        addi r11,this,0xB0 ; vrlimi128 v13,<0.0 splat>,8,0 ; stvx r11
    //                                            -- write 0.0 into lane 0 (.x = clutch factor) of
    //                                               mvClutchFactor_RPM_CurrentGearChangeTime,
    //                                               preserving the other lanes.
    //        stw <1>,0xC0(this)                  -- mu8CurrentGear = KU8_FIRST_GEAR (1).
    //        bl Reset(0.0)                       -- seed the running state from a 0 wheel velocity.
    //        li r3,1                             -- return true.
    // ---------------------------------------------------------------------------------------
    bool Engine::Prepare(const EngineAttribs* lpAttribs)
    {
        std::memcpy(&mAttribs, lpAttribs, sizeof(EngineAttribs));   // 160-byte attribs copy

        mvClutchFactor_RPM_CurrentGearChangeTime.x = 0.0f;          // clutch factor lane -> 0
        mu8CurrentGear = KU8_FIRST_GEAR;

        Reset(VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f });
        return true;
    }

    // ---------------------------------------------------------------------------------------
    // ComputeGear  @0x825CF010
    //   The automatic gearbox. The X360 logic, instruction-for-instruction:
    //     if ( !(drive >= -0.01) ) return 0;                       // reverse/neutral sentinel
    //     result = 1;
    //     metric(g) = | drive * gearRatio[g] * Differential * KF_RADIANS_PER_SEC_TO_RPM |
    //     if ( metric(1) > gearUpRPM[1] )                          // first up-shift gate (g=1)
    //        do { if (g>=5) break; ++result; g=result;
    //             } while ( metric(g) > gearUpRPM[g] );
    //     return result;
    //   Lane reads: Differential @+0x10.x, gearRatio @gear[g].x (`vspltw v12,v12,0`), gearUpRPM
    //   @gear[g].z (`vspltw v10,v10,2`). gear[g] is at `16*(g+4) + this` -- the `+4` is the 0x40
    //   base of mAttribs.mavGearRatios.
    //
    // THREE CORRECTIONS 2026-08-03, all asm-derived:
    //   * the metric is |.| -- `vandc v12,v12,vslw(vspltisw(-1),...)` clears the sign bit before
    //     `vcmpgtfp.`. The previous body dropped it. It matters: gear ratio 0 is NEGATIVE (-2.5).
    //   * the multiply order is `drive * ratio * differential * metric` (0x825CF090..0x825CF09C),
    //     not `drive * differential * ratio * metric`.
    // * KF_RPM_GEAR_METRIC was a FLAGGED 0.0f placeholder for the un-homed unk_82FB9110. With
    //     it zero the metric is IDENTICALLY ZERO, `metric > gearUpRPM` is never true and the
    //     gearbox is welded in gear 1 forever. The real value is 9.54929638 (see the anon
    //     namespace at the top of this file for how it was recovered).
    //
    // The parameter is a VecFloat, not an f32: the asm compares and multiplies register **v1**
    //   (`vcmpgefp. v0, v1, v0` / `vmulfp128 v12, v1, v12`), which is the first VECTOR argument.
    //   PPC would pass an `f32` in f1. Reset @0x825CF130 -- its only in-tree caller -- forwards its
    //   own VecFloat argument untouched, exactly as the console does.
    //   FLAG: the per-gear lane->field mapping (ratio/torqueScale/gearUpRPM) is the findings-doc's
    //   "unresolved" gear semantics; reproduced here exactly as the asm indexes them.
    // ---------------------------------------------------------------------------------------
    s32 Engine::ComputeGear(VecFloat lvfEngineDrive) const
    {
        if (!(lvfEngineDrive.x >= KF_REVERSE_DRIVE_THRESHOLD))
            return KU8_REVERSE_GEAR;

        const f32 lfDifferential = mAttribs.GetDifferential();

        s32 liGear = KU8_FIRST_GEAR;   // 1

        // metric for the first gate uses gear 1's ratio/threshold.
        f32 lfMetric = std::fabs(lvfEngineDrive.x * mAttribs.GetGearRatio(liGear)
                                 * lfDifferential * KF_RADIANS_PER_SEC_TO_RPM);

        if (lfMetric > mAttribs.GetGearUpRPM(liGear))
        {
            do
            {
                if (liGear >= KU8_HIGHEST_GEAR)
                    break;
                ++liGear;
                lfMetric = std::fabs(lvfEngineDrive.x * mAttribs.GetGearRatio(liGear)
                                     * lfDifferential * KF_RADIANS_PER_SEC_TO_RPM);
            }
            while (lfMetric > mAttribs.GetGearUpRPM(liGear));
        }

        return liGear;
    }

    // ---------------------------------------------------------------------------------------
    // Reset  @0x825CF130 .. 0x825CF274 (82 items)
    //
    // EXPORT-SET HOLE -- the FOURTH confirmed one. There is no
    // `.ida-exports/BURNOUT_X360_ARTIST.XEX/0x825CF130.json`; the hole is visible from the index
    // alone (ComputeGear @0x825CF010 is 72 instrs, so it ends exactly at 0x825CF130, and the next
    // indexed symbol is InitializeFromAttribs @0x825CF278). Body pulled with headless IDA 9.3.
    //
    // Seed the running state from a wheel angular velocity. Store order is the asm's:
    //     [this+0xA0].x = 0                       EngineDrive
    //     [this+0xA0].y = 0                       ReactionTorque
    //     [this+0xA0].w = 0                       ClutchDelay
    //     [this+0xB0].x = 0                       ClutchFactor
    //     [this+0xA0].z = unk_82FB9B10            FlyWheelAngularVelocity (idle: 1000 RPM in rad/s)
    //     bl ComputeGear                          (v1 forwarded unchanged)
    //     [this+0xC0]   = gear
    //     [this+0xB0].y = |wheelOmega * gearRatio[gear] * Differential * unk_82FB9110|   RPM
    //     [this+0xC4] = [this+0xC5] = 1           mbAllowToChangeUpGear / DownGear
    //     [this+0xB0].z = flt_82004A20 = 10.0     CurrentGearChangeTime
    //
    // The RPM expression is the same rad/s -> RPM chain ComputeGear uses, which is the role check
    // on unk_82FB9110: engine RPM = |wheel rad/s * gearing * 60/(2*pi)|.
    // ---------------------------------------------------------------------------------------
    void Engine::Reset(VecFloat lvfWheelAngularVelocity)
    {
        mvEngineDrive_ReactionTorque_FlyWheelAngularVelocity_ClutchDelay.x = 0.0f;   // EngineDrive
        mvEngineDrive_ReactionTorque_FlyWheelAngularVelocity_ClutchDelay.y = 0.0f;   // ReactionTorque
        mvEngineDrive_ReactionTorque_FlyWheelAngularVelocity_ClutchDelay.w = 0.0f;   // ClutchDelay
        mvClutchFactor_RPM_CurrentGearChangeTime.x = 0.0f;                           // ClutchFactor
        mvEngineDrive_ReactionTorque_FlyWheelAngularVelocity_ClutchDelay.z =
            KF_IDLE_FLYWHEEL_ANGULAR_VELOCITY;

        const s32 liGear = ComputeGear(lvfWheelAngularVelocity);
        mu8CurrentGear = static_cast<u32>(liGear);

        mvClutchFactor_RPM_CurrentGearChangeTime.y =
            std::fabs(lvfWheelAngularVelocity.x * mAttribs.GetGearRatio(liGear)
                      * mAttribs.GetDifferential() * KF_RADIANS_PER_SEC_TO_RPM);

        mbAllowToChangeUpGear   = true;
        mbAllowToChangeDownGear = true;

        mvClutchFactor_RPM_CurrentGearChangeTime.z = KF_RESET_CURRENT_GEAR_CHANGE_TIME;
    }

    // ---------------------------------------------------------------------------------------
    // GetMaxWheelAngularVelocity  @0x825BFDA0
    //   The rev limiter mapped through the current gearing. The X360:
    //     denom = gearRatio[mu8CurrentGear] * Differential            (asserted non-zero)
    //     result = MaxRPM / (gearRatio * Differential * flt_82F2A3E0) (vrefp + 2 Newton refinements)
    //   stored broadcast across the caller's Vector4 result (stvx128 into the sret buffer).
    //   (mu8CurrentGear @+0xC0; gear[curr] @ 16*(curr+4)+this; Differential @+0x10.x; MaxRPM @+0x20.z.)
    //
    //   The X360 emits an inlined RwMathVPU::IsZero(denom) assert guard first: it splats the
    //   |denom| > epsilon (stru_8208F620) compare into an int and cmpwi/bne to the assert.
    //
    //   FLAG (assert message): the rodata string aRwmathvpuIszer_1 is TRUNCATED in the export
    //   ('!RwMathVPU::IsZero(mAttribs.GetGearRati'...). Only the attested prefix is reproduced; the
    //   tail past the truncation is NOT recoverable, so it is NOT fabricated here.
    //
    // RESOLVED 2026-08-03: the denominator's extra factor flt_82F2A3E0 was a FLAGGED 1.0f
    //   placeholder. It is the SAME constant ComputeGear and Reset use -- 9.54929638 == 60/(2*pi)
    //   -- and it is not un-homed at all: it sits in `.data` but IS initialised in the image (raw
    //   41 18 C9 EB) and has no writer among its five xrefs. With the placeholder at 1.0 the rev
    //   limiter returned a wheel-speed ceiling 9.55x too high. Dividing MaxRPM by it converts RPM
    //   to rad/s before the gearing divide, which is exactly what the member's name asks for.
    // ---------------------------------------------------------------------------------------
    Vector4 Engine::GetMaxWheelAngularVelocity() const
    {
        const f32 lfDifferential = mAttribs.GetDifferential();
        const f32 lfGearRatio    = mAttribs.GetGearRatio(static_cast<s32>(mu8CurrentGear));
        const f32 lfMaxRPM       = mAttribs.GetMaxRPM();

        // asm: the stru_8208F620 vcmpgtfp select feeding the cmpwi/bne is the inlined
        // RwMathVPU::IsZero(gearRatio * Differential) assert guard.
        // FLAG: rodata message TRUNCATED in the export; reproduced verbatim as attested.
        const f32 lfAssertDenom = lfGearRatio * lfDifferential;
        CGS_ASSERT(lfAssertDenom != 0.0f,
                   "!RwMathVPU::IsZero(mAttribs.GetGearRati");

        const f32 lfDenominator = lfAssertDenom * KF_RADIANS_PER_SEC_TO_RPM;
        const f32 lfResult      = (lfDenominator != 0.0f) ? (lfMaxRPM / lfDenominator) : 0.0f;

        return Vector4{ lfResult, lfResult, lfResult, lfResult };
    }

    // ---------------------------------------------------------------------------------------
    // Update  @0x825CB288 -- THE POWERTRAIN TORQUE CORE.  BODIED 2026-08-09 (powertrain wave).
    //
    // PROVENANCE (three witnesses, cross-checked lane by lane):
    //   1. X360 @0x825CB288 (3937 asm lines): a debug Opt-vs-Unopt harness -- ONE algorithm run
    //      in two register files (a branchy member-writing leg + a branchless vsel leg),
    //      cross-asserted with tolerance flt_82002138=0.01 at the six source-line clusters the
    //      DWARF names (Engine.cpp:278-285/333-344/400-411/489-500/541-551/691-700, "Mismatch:
    //      Opt/Unopt ... Tell Graham D and include the TTY!"). The mismatches it hunts are the
    //      vrefp+Newton reciprocal vs true fdivs numerics, not algorithm forks. The epilogue
    //      commits the BRANCHLESS leg's registers; this body reproduces exactly that leg, with
    //      the branchy leg as the shape check.
    //   2. BPR x86 twin sub_BA63A0 (harness compiled out; found by call-graph descent
    //      RaceCarPhysics::UpdateDriving @0x79C4F00 -> j_UpdateEngine -> UpdateEngine @0x7A28210
    //      -> ApplyEngineForces @0x79DFB80; caller set == X360's two callers). Same Engine layout
    //      (a1[10]=+0xA0, a1[11]=+0xB0, a1[12]=+0xC0, gears at a1[g+4]). ALGORITHM oracle only;
    //      every constant value below was pinned from the X360 image.
    //   3. DWARF (DecFIGS PS3) local names + file-scope constant names, used verbatim.
    //
    //   Parameter mapping (X360 register file): v1 wheelAngVel, v2 gas, v3 brake, r4 handbrake,
    //   v4 steering, v5 rearWheelRadius (DEAD -- overwritten before any read, both platforms;
    //   AS-SHIPPED), r5 allowReverseDrive, v6 forwardSpeed, v7 timeStep.
    //
    //   Stage order is the console's: timer -> torque/wheel-speed -> clutch cuts -> snap-to-first
    //   -> low-speed first-gear lock -> flywheel -> gear-down RPM -> gear FSM -> engine drive ->
    //   writeback. Lanes NOT written: mvEngineDrive...y (ReactionTorque) and .w (ClutchDelay);
    //   mbAllowToChangeDownGear is only re-stored with its entry value on the console (a no-op,
    //   elided here); mbAllowToChangeUpGear IS cleared by the lock block (the epilogue does not
    //   restore it -- UpdateDriving re-arms both flags every frame via SetAllowGearChanges).
    //
    // FLAG (BPR divergence, X360 wins): the flywheel-friction clamp uses the freshly
    //   INTEGRATED flywheel velocity in `min(fly*sgn, friction*sgn*dt)` on X360 -- the branchy
    //   leg reads the member back at 0x825CD2C0/0x825CD2C8, i.e. after the lerp+torque step and
    //   before the friction bleed; the BPR twin reads its still-unwritten member, i.e. the
    //   PRE-frame value. Reproduced as the X360 shipped it.
    //   (Precision note, 2026-09-03 audit: the BRANCHLESS leg's `fly*sgn` term reads the member
    //   at 0x825CD3DC, which by then also carries the branchy leg's [idle, MaxRPM] clamp. The two
    //   legs therefore agree only while `fly > 2*friction*dt` -- which the 104.72 rad/s idle floor
    //   guarantees in every reachable state, and which is exactly what the harness's 0.01-tolerance
    //   assert at 0x825CEAE4 checks every frame. The branchy form is the one written here.)
    //
    // RE-DECODED FROM THE RAW IMAGE WORDS 2026-09-03 (drive-spine 1:1 audit). Every VMX128
    //   operand was taken from the encoding, not from IDA's print -- VD = bits 6-10 | bits<<5 of
    //   the word's bits 2-3, VB = bits 16-20 | (w&3)<<5, VA = bits 11-15 | bit5<<5 | bit10<<6
    //   (fitted over 265 three-operand VMX128 instructions in this function). The two fused forms
    //   that IDA prints with a phantom fourth operand were pinned by their Newton-Raphson idiom:
    //   `vnmsubfp128 vD,vA,vB` is `vD = vD - vA*vB` and `vmaddfp128 vD,vA,vB` is `vD = vA*vB + vD`,
    //   while the single `vmaddcfp128` @0x825CB7F0 is `vD = vA*vD + vB`. Result: 18 stages, all 21
    //   member stores, and every one of the ~30 constants re-read from the image; ONE divergence
    //   found (the gear-FSM chain flattening below, @0x825CE460).
    // ---------------------------------------------------------------------------------------
    void Engine::Update(VecFloat lvfWheelAngularVelocity, VecFloat lvfGas, VecFloat lvfBrake,
                        bool lbHandBrake, VecFloat lvfSteering, VecFloat /*lvfRearWheelRadius*/,
                        bool lbAllowReverseDrive, VecFloat lvfForwardSpeed, VecFloat lvfTimeStep)
    {
        const f32 lfGas          = lvfGas.x;
        const f32 lfBrake        = lvfBrake.x;
        const f32 lfSteering     = lvfSteering.x;
        const f32 lfForwardSpeed = lvfForwardSpeed.x;
        const f32 lfTimeStep     = lvfTimeStep.x;

        s32  liGear    = static_cast<s32>(mu8CurrentGear);
        bool lbAllowUp = mbAllowToChangeUpGear;
        const bool lbAllowDown = mbAllowToChangeDownGear;

        // -- Timer: advance and saturate (vaddfp + vminfp vs unk_82FB8420=10.0; the console
        //    stores this to the member immediately, then overwrites it with 0 on a shift).
        f32 lfTimer = std::min(mvClutchFactor_RPM_CurrentGearChangeTime.z + lfTimeStep,
                               KVF_MAX_CURRENT_GEAR_CHANGE_TIME);

        // -- Torque: quadratic torque curve sampled at RPM / (2 * TorqueFallOffRPM), scaled by
        //    the current gear's torque scale and the gas (vrefp+NR of 2*FallOff; De Casteljau
        //    over mTorqueCurve; * gear[g].y ; * v2).
        const f32 lfEngineInvInertia = 1.0f / mAttribs.GetFlyWheelInertia();
        const f32 lfCurveT = mvClutchFactor_RPM_CurrentGearChangeTime.y
                             / (2.0f * mAttribs.GetTorqueFallOffRPM());
        f32 lfTorque = mAttribs.mTorqueCurve
                           .GetInterped(VecFloat{ lfCurveT, lfCurveT, lfCurveT, lfCurveT }).x
                       * mAttribs.GetTorqueScale(liGear) * lfGas;

        // -- Wheel speed seen through the current gearing (ComputeRpm): |v * ratio * diff| in
        //    RPM (unk_82FB9110 = 9.549296) and in rad/s (unk_82FB8B20 = 0.104719755).
        const f32 lfWheelRPM = std::fabs(lvfWheelAngularVelocity.x * mAttribs.GetGearRatio(liGear)
                                         * mAttribs.GetDifferential() * KF_RADIANS_PER_SEC_TO_RPM);
        const f32 lfWheelAngularVelocity = lfWheelRPM * KF_RPM_TO_RADIANS_PER_SEC;

        // -- Clutch cuts, in the console's order. Each state independently zeroes the clutch.
        f32 lfClutch = mvClutchFactor_RPM_CurrentGearChangeTime.x;
        if (lfWheelAngularVelocity < KF_IDLE_FLYWHEEL_ANGULAR_VELOCITY && liGear == 1
            && lfGas < KVF_UPDATEENGINE_STOP_GAS)
            lfClutch = 0.0f;                                       // idling in neutral
        if (lfGas <= KF_UPDATE_EPSILON && lfGas >= -KF_UPDATE_EPSILON
            && lfBrake > KVF_UPDATEENGINE_BRAKING_THRESHOLD && liGear != 0)
            lfClutch = 0.0f;                                       // just braking
        if (lbHandBrake && lfForwardSpeed <= KVF_UPDATEENGINE_HANDBRAKE_MAX_SPEED)
            lfClutch = 0.0f;                                       // handbraking
        if (mAttribs.GetGearChangeTime() > lfTimer)
            lfClutch = 0.0f;                                       // still changing gear

        // -- Snap back to first: clutch fully out, above first, wheels below twice the idle
        //    flywheel speed (the console computes 2*unk_82FB9B10 inline; the vector leg's slot
        //    unk_82FB9360 is the same product, DWARF KVF_TWO_FLY_WHEEL_IDLE_VELOCITY).
        if (lfClutch == 0.0f && liGear > 1
            && lfWheelAngularVelocity < 2.0f * KF_IDLE_FLYWHEEL_ANGULAR_VELOCITY)
            liGear = 1;

        // -- Low-speed first-gear lock: hard steering, braking, or rolling backwards below the
        //    lock speed pins first gear, forbids upshifting, and burns the clutch.
        f32 lfBurnClutch = 0.0f;
        if (lfForwardSpeed < KVF_LOW_SPEED_FIRST_GEAR_LOCK_THRESHOLD && liGear != 0
            && (std::fabs(lfSteering) > KVF_STEERING_LOCK_THRESHOLD
                || lfBrake > KVF_BRAKE_LOCK_THRESHOLD
                || lfForwardSpeed < 0.0f))
        {
            liGear    = 1;
            lbAllowUp = false;
            mbAllowToChangeUpGear = false;   // the branchy console leg writes the member here
            if (lfForwardSpeed > 0.0f)
            {
                lfBurnClutch = 1.0f
                    - lfForwardSpeed * KVF_RECIP_LOW_SPEED_FIRST_GEAR_LOCK_THRESHOLD;
            }
            else
            {
                lfBurnClutch = KVF_BURN_CLUTCH_IF_NOT_GOING_FORWARDS;
                lfTorque    *= 2.0f;         // reversing power-through (GetVecFloat_Two)
            }
        }

        // -- Flywheel: when no gear change is in flight, drag the flywheel toward the wheel
        //    speed through the (burn-weakened) clutch, integrate the engine torque, then bleed
        //    friction without crossing zero. Always clamp to [idle, MaxRPM in rad/s].
        f32 lfFlyWheel = mvEngineDrive_ReactionTorque_FlyWheelAngularVelocity_ClutchDelay.z;
        if (lfTimer >= mAttribs.GetGearChangeTime())
        {
            const f32 lfLerp = (1.0f - lfBurnClutch) * mAttribs.GetTransmissionEfficiency()
                               * lfClutch;
            lfFlyWheel += (lfWheelAngularVelocity - lfFlyWheel) * lfLerp;
            lfFlyWheel += lfTorque * lfEngineInvInertia * lfTimeStep;

            const f32 lfSgn = BpSgn(lfFlyWheel);   // sign AFTER integration (see FLAG above)
            lfFlyWheel -= std::min(lfFlyWheel * lfSgn,
                                   mAttribs.GetFlyWheelFriction() * lfSgn * lfTimeStep);
        }
        lfFlyWheel = std::min(mAttribs.GetMaxRPM() * KF_RPM_TO_RADIANS_PER_SEC,
                              std::max(KF_IDLE_FLYWHEEL_ANGULAR_VELOCITY, lfFlyWheel));
        const f32 lfRPM = lfFlyWheel * KF_RADIANS_PER_SEC_TO_RPM;

        // -- Gear-down RPM: project the current gear's up-shift RPM into the lower gear's rev
        //    space and subtract the 50-RPM-per-ratio hysteresis margin (writes the ATTRIBS lane,
        //    exactly as shipped -- Update mutates its own tuning block).
        f32 lfGearDownRPM = mAttribs.GetGearDownRPM();
        if (liGear > 0)
        {
            lfGearDownRPM = mAttribs.GetGearUpRPM(liGear) * mAttribs.GetGearRatio(liGear)
                                / mAttribs.GetGearRatio(liGear - 1)
                            - KF_DOWNSHIFT_FACTOR * mAttribs.GetGearRatio(liGear - 1);
        }

        // -- The gear state machine: ONE FLAT else-if chain of five links, not a nest.
        //
        // ⚠️ CORRECTED 2026-09-03 (drive-spine audit, re-decoded from the raw image words). The
        // first link used to be written as a NESTED `if (liGear == 0) { if (brake && speed) … }`,
        // which silently swallowed links 2-5 for the whole of reverse gear. The console does not:
        // ALL THREE exits of link 1 -- `bne cr6` on gear != 0 @0x825CE468, `beq cr6` on
        // brake <= 0.1 @0x825CE4A0, and `beq cr6` on fwd <= -5 @0x825CE4E0 -- branch to the SAME
        // label 0x825CE4FC, which is link 2. (Branch displacements read out of the encodings:
        // 409A0094 / 419A005C / 419A001C, all landing on 0x825CE4FC.) The branchless leg says the
        // same thing without any branching at all: it ANDs the three masks into one predicate --
        // `vand128 v8, v120(brake>0.1), v7(fwd>-5)` then `vand128 v0, v110(gear==0), v8`
        // @0x825CE7F0..0x825CE7F8 -- and falls into link 2 when that single predicate is false.
        // So the original source is one `&&`, and the nest was a transcription defect.
        //
        // What it cost: in reverse (gear 0) with the exit test failing, link 3 -- "the clutch is
        // out, bite again once the revs pass a quarter of the up-shift RPM" -- is the ONLY other
        // link that can fire (link 2 needs gear 1; link 4 rejects gear 0 at 0x825CE6D4; link 5
        // needs gear > 1). Entering reverse parks the gear-change timer at 0, so on the very next
        // frame the "still changing gear" cut zeroes the clutch; with the nest in place nothing
        // could ever set it back, because link 3 was unreachable in reverse. The clutch stayed at
        // 0 and mEngineDrive = torque * 0 * ratio * diff, i.e. reverse had NO drive at all once
        // the clutch had been cut. Faithful now.
        if (liGear == 0
            && lfBrake > KVF_BRAKE_LOCK_THRESHOLD
            && lfForwardSpeed > KVF_REVERSING_POWER_THROUGH_THESHOLD)
        {
            // Leave reverse: brake pressed while rolling forwards fast enough.
            liGear  = 1;
            lfTimer = 0.0f;
        }
        else if (liGear == 1 && lbAllowReverseDrive
                 && lfBrake > KVF_START_REVERSING_BRAKE_THRESHOLD
                 && lfGas * lfBrake < KVF_START_REVERSING_GASBRAKE_THRESHOLD)
        {
            // Enter reverse: braking hard with (almost) no gas from first gear.
            liGear  = 0;
            lfTimer = 0.0f;
            if (!lbHandBrake)
                lfClutch = 1.0f;
        }
        else if (lfClutch <= KF_UPDATE_EPSILON && lfClutch >= -KF_UPDATE_EPSILON)
        {
            // Clutch is out: bite again once the revs pass a quarter of the up-shift RPM.
            if (lfRPM > mAttribs.GetGearUpRPM(liGear) * KF_QUARTER && !lbHandBrake)
                lfClutch = 1.0f;
        }
        else if (lfRPM > mAttribs.GetGearUpRPM(liGear)
                 && liGear >= 1 && liGear < KU8_HIGHEST_GEAR && lbAllowUp)
        {
            ++liGear;                                              // up-shift
            lfTimer = 0.0f;
        }
        else if (lfGearDownRPM > lfRPM && liGear > 1 && lbAllowDown)
        {
            --liGear;                                              // down-shift
            lfTimer = 0.0f;
        }

        // -- Engine drive: what ApplyEngineForcesOntoWheels distributes to the driven wheels.
        const f32 lfEngineDrive = lfTorque * lfClutch * mAttribs.GetGearRatio(liGear)
                                  * mAttribs.GetDifferential();

        // -- Writeback (the console epilogue's exact lane set).
        mu8CurrentGear = static_cast<u32>(liGear) & 0xFF;          // stw (clrlwi r20,24)
        mAttribs.SetGearDownRPM(VecFloat{ lfGearDownRPM, lfGearDownRPM,
                                          lfGearDownRPM, lfGearDownRPM });
        mvClutchFactor_RPM_CurrentGearChangeTime.y = lfRPM;
        mvEngineDrive_ReactionTorque_FlyWheelAngularVelocity_ClutchDelay.x = lfEngineDrive;
        mvEngineDrive_ReactionTorque_FlyWheelAngularVelocity_ClutchDelay.z = lfFlyWheel;
        mvClutchFactor_RPM_CurrentGearChangeTime.x = lfClutch;
        mvClutchFactor_RPM_CurrentGearChangeTime.z = lfTimer;

        // ---- [powertrain] PC bring-up instrument -- DELETE WHEN the drivetrain is right -------
        // OPT-IN (BRN_ENGINE_PROBE=1) so a default run and every golden gate are byte-identical
        // to a build without it. One line every 30 calls: the whole torque chain plus the three
        // per-gear attribs lanes it read, so a wrong lane shows up as a wrong NUMBER, not a guess.
        {
            // ⚠️ THE VALUE IS A SAMPLING PERIOD IN PEDALLED CALLS, not a boolean (changed
            // 2026-09-03, same convention as BRN_DEFORM_TRACE / -TractionProbe). It used to be
            // an on/off arm with the period hard-wired to 30 -- one sample every half second,
            // which is too coarse to resolve a two-second clutch event: a state that is entered
            // and left inside four samples cannot be told apart from a sampling artefact.
            // 0 or unset == fully off, exactly as before.
            static s32 siPowertrainPeriod = -1;
            if (siPowertrainPeriod < 0)
            {
                const char* lpcEnv = getenv("BRN_ENGINE_PROBE");
                siPowertrainPeriod = (lpcEnv != 0) ? atoi(lpcEnv) : 0;
                if (siPowertrainPeriod < 0) { siPowertrainPeriod = 0; }
            }
            // Sample on a counter that only advances for PEDALLED calls. A single global
            // counter aliases: several vehicles call Update in a fixed order every tick, so
            // `count % 30` lands on the same slot forever and the player is never sampled.
            //
            // ⚠️⚠️ WIDENED 2026-09-03 (reverse-gear film wave). This used to gate on
            // `lfGas > 0.0f` ALONE -- which makes the probe STRUCTURALLY BLIND to the one
            // state the gear FSM's first link governs. Reverse is entered and held on the
            // BRAKE with no gas at all (link 2 needs `lfGas * lfBrake < 0.05`), so every
            // reverse frame had gas == 0 and the probe printed nothing: an instrument that
            // cannot observe the behaviour it is pointed at. Now any pedal input samples,
            // and reverse gear samples even coasting, so the clutch/drive collapse the flat
            // chain fixes is visible frame by frame rather than inferred.
            static u32 suPowertrainCount = 0;
            const bool lbUnderPower = (lfGas > 0.0f) || (lfBrake > 0.0f) || (liGear == 0);
            if (lbUnderPower)
                ++suPowertrainCount;
            if (siPowertrainPeriod > 0 && lbUnderPower
                && (suPowertrainCount % static_cast<u32>(siPowertrainPeriod)) == 0u
                && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[powertrain] n " << static_cast<s32>(suPowertrainCount)
                    << " present " << static_cast<s32>(renderengine::guPresentCount)
                    << " gear " << liGear
                    << " rpm " << lfRPM
                    << " fly " << lfFlyWheel
                    << " clutch " << lfClutch
                    << " torque " << lfTorque
                    << " curveT " << lfCurveT
                    << " drive " << lfEngineDrive
                    << " gas " << lfGas
                    << " brake " << lfBrake
                    << " allowRev " << (lbAllowReverseDrive ? 1 : 0)
                    << " wOmega " << lvfWheelAngularVelocity.x
                    << " wRPM " << lfWheelRPM
                    << " fwd " << lfForwardSpeed
                    << " timer " << lfTimer
                    << " | ratio " << mAttribs.GetGearRatio(liGear)
                    << " upRPM " << mAttribs.GetGearUpRPM(liGear)
                    << " tScale " << mAttribs.GetTorqueScale(liGear)
                    << " diff " << mAttribs.GetDifferential()
                    << " maxRPM " << mAttribs.GetMaxRPM()
                    << " fallOff " << mAttribs.GetTorqueFallOffRPM()
                    << " inertia " << mAttribs.GetFlyWheelInertia()
                    << " friction " << mAttribs.GetFlyWheelFriction()
                    << " gct " << mAttribs.GetGearChangeTime()
                    << " transEff " << mAttribs.GetTransmissionEfficiency()
                    << " burn " << lfBurnClutch
                    << "\n";
            }
        }
        // ---- end [powertrain] -----------------------------------------------------------------
    }
}
}
