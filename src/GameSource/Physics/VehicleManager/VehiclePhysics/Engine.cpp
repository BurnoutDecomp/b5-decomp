#include "GameSource/Physics/VehicleManager/VehiclePhysics/Engine.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cmath>     // std::fabs
#include <cstddef>   // offsetof
#include <cstring>   // std::memcpy

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
    // ⭐ RECOVERED 2026-08-03. Two `.data` slots that read ALL ZEROS in the image and are filled by
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
    //   ⚠️ THREE CORRECTIONS 2026-08-03, all asm-derived:
    //   * the metric is |.| -- `vandc v12,v12,vslw(vspltisw(-1),...)` clears the sign bit before
    //     `vcmpgtfp.`. The previous body dropped it. It matters: gear ratio 0 is NEGATIVE (-2.5).
    //   * the multiply order is `drive * ratio * differential * metric` (0x825CF090..0x825CF09C),
    //     not `drive * differential * ratio * metric`.
    //   * ⭐ KF_RPM_GEAR_METRIC was a FLAGGED 0.0f placeholder for the un-homed unk_82FB9110. With
    //     it zero the metric is IDENTICALLY ZERO, `metric > gearUpRPM` is never true and the
    //     gearbox is welded in gear 1 forever. The real value is 9.54929638 (see the anon
    //     namespace at the top of this file for how it was recovered).
    //
    //   ⚠️ The parameter is a VecFloat, not an f32: the asm compares and multiplies register **v1**
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
    // ⚠️ EXPORT-SET HOLE -- the FOURTH confirmed one. There is no
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
    //   ⭐ RESOLVED 2026-08-03: the denominator's extra factor flt_82F2A3E0 was a FLAGGED 1.0f
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
}
}
