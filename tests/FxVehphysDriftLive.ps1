# FX-VEHPHYS (crash parity 2026-09-23) live witness for G51-D1: the drift speed-maintain impulse
# of VehiclePhysics::MaintainDriftSpeed @0x825D2270 carries mLinearVelocity / speedParam (the
# console's 0x825D2398 vrefp of the PRE-damping speed times the damped +0x50 velocity), not a unit
# vector. Handbrake-initiated slides on the open road outside the stunt junction, released onto the
# throttle with the lock held, reach the success arm (maintain flag, speed <= MaintainedSpeed,
# traction, gas >= 0.3, no handbrake, valid ground). The opt-in [drift-maintain] witness
# (BRN_DRIFT_PROBE=1, player car, first 60 lines) prints the term's length: below 1 whenever the
# drift's sideways damping has bitten, where the pre-fix Normalize made it exactly 1.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxVehphysDriftLive.ps1
# What it proves: DISPATCH of the fixed arm plus no new asserts. The drive is wall-clock coupled,
# so the number of slides differs run to run; the gate only needs one.
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
        Teleport = '2641.5,1.3,-1723.8,169'
        SkipTrainingTip = $true
        MaxSeconds = 90
        ThrottleScript = '0:accel,12:accel+handbrake,13.2:accel,24:accel+handbrake,25.2:accel,36:accel+handbrake,37.2:accel,48:accel+handbrake,49.2:accel'
        SteerScript = '0:none,11.6:left,17:none,23.6:right,29:none,35.6:left,41:none,47.6:right,53:none'
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
