#include "GameSource/Physics/VehicleManager/VehiclePhysics/Engine.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // std::memcpy

// BrnPhysics::Vehicle::Engine -- the two ledger functions owned by the Vehicle-physics group.
// The X360 build is VMX128 inline asm; these are the de-SIMD'd named-member equivalents
// recovered store-for-store from the asm at 0x825F3EE8 (Construct) and 0x825F3F38 (Prepare).

namespace BrnPhysics
{
namespace Vehicle
{
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
    //   The automatic gearbox. The caller passes the current engine drive in v1 (= mvEngineDrive
    //   lane0). The X360 logic, store-for-store:
    //     if ( !(engineDrive >= -0.0099999998) ) return 0;          // reverse/neutral sentinel
    //     result = 1;
    //     metric(g) = engineDrive * Differential * gearRatio[g] * KF_RPM_GEAR_METRIC
    //     // walk up while the current gear's up-RPM threshold is exceeded, gears 1..5
    //     if ( metric(1) > gearUpRPM[1] )                            // first up-shift gate (g=1)
    //        do { if (g>=5) break; ++result; g=result;
    //             } while ( metric(g) > gearUpRPM[g] );
    //     return result;
    //   The -0.0099999998 is an INLINE literal (not a named .rdata symbol). KF_RPM_GEAR_METRIC is
    //   the un-homed multiply constant unk_82FB9110 (an rads<->RPM / metric scale); carried as an
    //   honest flagged placeholder (faithful-but-inert) per project rule -- the gear-walk structure,
    //   the lane reads (Differential @+0x10.x, gearRatio @gear[g].x, gearUpRPM @gear[g].z) and the
    //   reverse sentinel are EXACT; the numeric metric stays scaled by 0 until the constant is
    //   recovered from the XEX data (so by default the box stays in gear 1, never up-shifts).
    //   FLAG: the per-gear lane->field mapping (ratio/torqueScale/gearUpRPM) is the findings-doc's
    //   "unresolved" gear semantics; reproduced here exactly as the asm indexes them.
    // ---------------------------------------------------------------------------------------
    s32 Engine::ComputeGear(f32 lfEngineDrive) const
    {
        static const f32 KF_REVERSE_DRIVE_THRESHOLD = -0.0099999998f;   // inline literal
        static const f32 KF_RPM_GEAR_METRIC         = 0.0f;             // FLAG: un-homed unk_82FB9110

        if (!(lfEngineDrive >= KF_REVERSE_DRIVE_THRESHOLD))
            return KU8_REVERSE_GEAR;

        const f32 lfDifferential = mAttribs.GetDifferential();

        s32 liGear = KU8_FIRST_GEAR;   // 1

        // metric for the first gate uses gear 1's ratio/threshold.
        f32 lfMetric = lfEngineDrive * lfDifferential
                     * mAttribs.GetGearRatio(liGear) * KF_RPM_GEAR_METRIC;

        if (lfMetric > mAttribs.GetGearUpRPM(liGear))
        {
            do
            {
                if (liGear >= 5)
                    break;
                ++liGear;
                lfMetric = lfEngineDrive * lfDifferential
                         * mAttribs.GetGearRatio(liGear) * KF_RPM_GEAR_METRIC;
            }
            while (lfMetric > mAttribs.GetGearUpRPM(liGear));
        }

        return liGear;
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
    //   FLAG (denominator factor): the reciprocal block multiplies in an extra constant flt_82F2A3E0
    //   (un-homed .rdata, almost certainly RPM->rad/s = 2*pi/60). Its numeric value is not in the
    //   export, so it is carried as a FLAGGED placeholder (faithful-but-inert); the asserted
    //   denominator (gearRatio*Differential) is exact.
    // ---------------------------------------------------------------------------------------
    Vector4 Engine::GetMaxWheelAngularVelocity() const
    {
        static const f32 KF_RPM_TO_WHEEL_OMEGA = 1.0f;   // FLAG: un-homed flt_82F2A3E0 denom factor

        const f32 lfDifferential = mAttribs.GetDifferential();
        const f32 lfGearRatio    = mAttribs.GetGearRatio(static_cast<s32>(mu8CurrentGear));
        const f32 lfMaxRPM       = mAttribs.GetMaxRPM();

        // asm: the stru_8208F620 vcmpgtfp select feeding the cmpwi/bne is the inlined
        // RwMathVPU::IsZero(gearRatio * Differential) assert guard.
        // FLAG: rodata message TRUNCATED in the export; reproduced verbatim as attested.
        const f32 lfAssertDenom = lfGearRatio * lfDifferential;
        CGS_ASSERT(lfAssertDenom != 0.0f,
                   "!RwMathVPU::IsZero(mAttribs.GetGearRati");

        const f32 lfDenominator = lfAssertDenom * KF_RPM_TO_WHEEL_OMEGA;
        const f32 lfResult      = (lfDenominator != 0.0f) ? (lfMaxRPM / lfDenominator) : 0.0f;

        return Vector4{ lfResult, lfResult, lfResult, lfResult };
    }
}
}
