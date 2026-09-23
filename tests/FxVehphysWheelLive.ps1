# FX-VEHPHYS (crash parity 2026-09-23) live witness for G54-D1: the airborne arm of
# VehiclePhysics::CalculateBodyVelocityAtWheelContact @0x825FB200 builds the lever arm from the
# body-local wheel position (mPosition, radius off .y) rotated into world space, with no
# translation -- not the streamed position minus mTransform.Pos() (an arm of about |worldPos|).
# The drive: three handbrake-at-full-lock stabs at speed on the open road by the stunt junction,
# which SPIN the car (run fxvehphys_drift/20260923_144455: yaw 2.6 rad/s at 80 mph, all four wheels
# past their adhesive limit), so wheels leave the road while the body is rotating. The opt-in
# [wheel-bpv] witness (BRN_WHEEL_BPV_PROBE=1, player car, first 40 airborne wheels with
# |omega| > 0.05) prints the arm the stored mBodyPointVelocity implies, |bpv - v| / |omega|, beside
# |pos| -- the arm the pre-fix body implied (thousands of metres here).
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxVehphysWheelLive.ps1
# What it proves: DISPATCH of the airborne arm on a rotating player car, a body-scale arm, and no
# new asserts. The old arm's effect was visual only (touch-down skid smoke / landing dust read
# WheelLite.mVelocity). The drive is wall-clock coupled; the gate needs one airborne wheel.
@{
    Name = 'fxvehphys_wheel_bpv'
    Area = 'physics/wheels'
    Bug = 'An airborne wheel''s body-point velocity must use the rotated body-local wheel position as its lever arm (G54-D1).'
    Frames = $false
    Run = @{
        Drive = $true
        MotionProbe = $true
        SkipIntro = $true
        AcceptGap = 1.0
        Teleport = '2641.5,1.3,-1723.8,169'
        SkipTrainingTip = $true
        MaxSeconds = 75
        ThrottleScript = '0:accel,12:accel+handbrake,13.2:accel,24:accel+handbrake,25.2:accel,36:accel+handbrake,37.2:accel'
        SteerScript = '0:none,11.6:left,17:none,23.6:right,29:none,35.6:left,41:none'
    }
    DiagEnv = 'BRN_WHEEL_BPV_PROBE=1'
    Checks = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
        @{ Kind = 'LogMatch'; Name = 'G54-D1 airborne arm dispatched on a rotating player car'
           Pattern = '\[wheel-bpv\] n \d+ w \d'; Expect = $true }
        @{ Kind = 'LogValue'; Name = 'implied lever arm is body-scale (the old arm implied |pos|, thousands of metres here)'
           Pattern = '\[wheel-bpv\] .* arm (?<v>[-\d.]+)'; Group = 'v'; Agg = 'all'; Min = 0.0; Max = 5.0 }
    )
}
