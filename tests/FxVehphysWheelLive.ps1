# FX-VEHPHYS (crash parity 2026-09-23) live witness for G54-D1: the airborne arm of
# VehiclePhysics::CalculateBodyVelocityAtWheelContact @0x825FB200 builds the lever arm from the
# body-local wheel position (mPosition, radius off .y) rotated into world space, with no
# translation -- not the streamed position minus mTransform.Pos() (an arm of about |worldPos|).
# The drive is FxVehphysDriftLive's (teleport + run-up + brake/steer slide); the opt-in
# [wheel-bpv] witness (BRN_WHEEL_BPV_PROBE=1, player car, first 40 airborne wheels) prints the arm
# that the stored mBodyPointVelocity implies, |bpv - v| / |omega| (only when |omega| > 0.05,
# else -1), next to |pos|, the arm the pre-fix body implied.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxVehphysWheelLive.ps1
# What it proves: DISPATCH of the airborne arm on the player car plus no new asserts; the effect
# of the old arm was visual only (touch-down skid smoke / landing dust read WheelLite.mVelocity).
$case = & (Join-Path $PSScriptRoot 'FxVehphysDriftLive.ps1')
$case.Name = 'fxvehphys_wheel_bpv'
$case.Area = 'physics/wheels'
$case.Bug = 'An airborne wheel''s body-point velocity must use the rotated body-local wheel position as its lever arm (G54-D1).'
$case.DiagEnv += ',BRN_WHEEL_BPV_PROBE=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'G54-D1 airborne arm dispatched on the player car'
       Pattern = '\[wheel-bpv\] n \d+ w \d'; Expect = $true }
    @{ Kind = 'LogValue'; Name = 'implied lever arm is body-scale (the old arm implied |pos|, thousands of metres here)'
       Pattern = '\[wheel-bpv\] .* arm (?<v>[-\d.]+)'; Group = 'v'; Agg = 'all'; Min = -1.0; Max = 5.0 }
)
$case
