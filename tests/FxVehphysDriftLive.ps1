# FX-VEHPHYS (crash parity 2026-09-23) live witness for G51-D1: the drift speed-maintain impulse
# of VehiclePhysics::MaintainDriftSpeed @0x825D2270 carries mLinearVelocity / speedParam (the
# console's 0x825D2398 vrefp of the PRE-damping speed times the damped +0x50 velocity), not a unit
# vector. The drive is the waterfront drift recipe of the drift-symmetry wave (dsw_right2: 63
# frames in drift state 1): a 7 s run-up due south at x 3392, then brake+gas with full right lock.
# Brake+steer latches the drift (CheckForEnteringDrift); held gas keeps MaintainDriftSpeed's gates
# open (maintain flag, speed <= MaintainedSpeed, traction, gas >= 0.3, no handbrake, valid ground).
# The opt-in [drift-maintain] witness (BRN_DRIFT_PROBE=1, player car, first 60 lines) prints the
# term's length: below 1 whenever the drift's sideways damping has bitten, where the pre-fix
# Normalize made it exactly 1. (A handbrake-at-full-lock schedule SPINS the car instead: run
# fxvehphys_drift/20260923_144455 never left drift state 0.)
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxVehphysDriftLive.ps1
# What it proves: DISPATCH of the fixed arm plus no new asserts. The drive is wall-clock coupled,
# so the drift length differs run to run; the gate only needs one maintain frame.
@{
    Name = 'fxvehphys_drift'
    Area = 'physics/drift'
    Bug = 'MaintainDriftSpeed must scale the velocity term by 1/speedParam, not normalise it (G51-D1).'
    Frames = $false
    Run = @{
        Drive = $true
        MotionProbe = $true
        SkipIntro = $true
        AcceptGap = 1.0
        Teleport = '3392,0.2,-1700,180'
        SkipTrainingTip = $true
        MaxSeconds = 55
        ThrottleScript = '0:accel,7:accel+brake,11:none'
        SteerScript = '0:none,7:right,11:none'
    }
    DiagEnv = 'BRN_DRIFT_PROBE=1'
    Checks = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
        @{ Kind = 'LogMatch'; Name = 'G51-D1 MaintainDriftSpeed success arm dispatched'
           Pattern = '\[drift-maintain\] n \d+ speedParam'; Expect = $true }
        @{ Kind = 'LogValue'; Name = 'velocity term length is |v|/speedParam, at most 1 (sane range)'
           Pattern = '\[drift-maintain\] .* velTerm (?<v>[-\d.]+)'; Group = 'v'; Agg = 'all'; Min = 0.5; Max = 1.0001 }
        @{ Kind = 'LogValue'; Name = 'velocity term below 1 on some frame (the pre-fix Normalize was exactly 1)'
           Pattern = '\[drift-maintain\] .* velTerm (?<v>[-\d.]+)'; Group = 'v'; Agg = 'min'; Max = 0.9999 }
    )
}
