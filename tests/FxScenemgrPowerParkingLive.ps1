# FX-SCENEMGR item 4 (crash parity 2026-09-24) -- live witness for the Power Parking CONSUMER: the
# embedded PowerParkingManager, RaceCarEntityModule::UpdatePowerParking @0x822FF5B0 (PostPhysicsUpdate
# 0x8230778C) and ProcessPowerParking @0x822CDF10 (PostSceneUpdate 0x822FE588), both gated
# `!mbIsInGameMode || meGameModeType == 15` -- i.e. free burn.
#
# The drive: free burn, teleported onto the street where FX-TRAFFIC3's parked-traffic run measured two
# parked cars inside 15 m (run fxtraffic3_parked_traffic/20260924_135053: count=2 closest 7.6 m with the
# player at x 1756..1747, z -2318..-2317, heading (-0.97, 0.25) = 284 deg). A run-up to above 15 m/s
# (KF_MIN_LINEAR_VELOCITY_TO_START_POWER_PARK, flt_82CDB4C4) then the handbrake (> 0.2, flt_82CDB4C8)
# STARTS a park; the car slides to rest beside the parked cars and the park ENDS (v <= 1.0, angular
# <= 0.2), DetermineOutcome grading it. The whole loop the item closes shows up in the log:
#   [power-park] start: park in progress ...                  UpdatePowerParking (the scorer's start)
#   [power-park] publish RaceCarToTrafficInterface bit 0 = 1   ProcessPowerParking's publish
#   [T-parked] published count=N ...                          the traffic producer, its gate now OPEN
#   [power-park] nearby parked data: count N players 0 ...    ProcessPowerParking -> the scorer
#   [power-park] end: outcome O rating R nearby parked N ...   the park's end and its grade
#   [power-park] result countdown over: ... events appended 1  E_EVENT_POWER_PARK_RESULT posted
# (the outcome needs >= 2 parked cars inside 15 m and a perpendicular distance under 3.0 -- a gameplay
# outcome of where the slide stops, so it is reported, not gated).
# Before the fix: no member, no bodies, no calls -- none of the [power-park] lines, and the traffic
# producer only ever logged "[T-parked] dispatched, mbPlayerIsPowerParking=0".
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxScenemgrPowerParkingLive.ps1
@{
    Name = 'fxscenemgr_power_parking'
    Area = 'racecar/power-parking'
    Bug  = 'Power Parking had no consumer: no PowerParkingManager in the race-car module, no UpdatePowerParking / ProcessPowerParking, so a park never started and the traffic producer''s gate never opened (FX-SCENEMGR item 4).'
    Frames = $false
    Run = @{
        Drive           = $true
        MotionProbe     = $true      # [motion] pose/speed trace: where the slide started and stopped
        SkipIntro       = $true
        AcceptGap       = 1.0
        SkipTrainingTip = $true
        MaxSeconds      = 85
        Teleport        = '1817.8,-3.2,-2333.7,284'
        # 0 s nudge (trips the teleport's 8 m arm) ; 3 s hold still while the street streams in ;
        # 9 s run-up (~4.3 s: ~19 m/s, ~40 m) ; 13.3 s handbrake + brake (the slide) ; 15.5 s handbrake alone (no reverse) ;
        # 22 s let go and watch the result countdown.
        ThrottleScript  = '0:accel,3:handbrake,9:accel,13.3:handbrake+brake,15.5:handbrake,22:none'
    }
    DiagEnv = 'BRN_POWER_PARK_DIAG=1,BRN_TRAFFIC_DIAG=1'
    Checks = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
        @{ Kind = 'LogMatch'; Name = 'UpdatePowerParking ran the scorer and a park STARTED (handbrake above 15 m/s)'
           Pattern = '\[power-park\] start: park in progress'; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'ProcessPowerParking published RaceCarToTrafficInterface bit 0'
           Pattern = '\[power-park\] publish RaceCarToTrafficInterface bit 0 = 1'; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'the traffic producer''s gate opened: parked data PUBLISHED (not the closed-gate line only)'
           Pattern = '\[T-parked\] published count='; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'ProcessPowerParking handed the parked data to the scorer'
           Pattern = '\[power-park\] nearby parked data: count \d+ players \d+'; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'the park ended and was graded'
           Pattern = '\[power-park\] end: outcome \d'; Expect = $true }
        @{ Kind = 'Script'; Name = 'INFO -- the grade, the parked cars and the result event (never fails)'; Script = {
            param($ctx)
            $ends = @($ctx.LogLines | Where-Object { $_ -match '\[power-park\] end: ' })
            $results = @($ctx.LogLines | Where-Object { $_ -match '\[power-park\] result countdown over' })
            $maxCount = 0
            foreach ($l in $ctx.LogLines) {
                if ($l -match '\[power-park\] nearby parked data: count (\d+)') { if ([int]$Matches[1] -gt $maxCount) { $maxCount = [int]$Matches[1] } }
            }
            $first = if ($ends.Count) { $ends[0].Trim() } else { 'no end line' }
            $res = if ($results.Count) { $results[0].Trim() } else { 'no result countdown (outcome stayed TO_BE_DETERMINED: fewer than 2 parked cars inside 15 m, or perp >= 3.0)' }
            @{ Pass = $true; Detail = "ends $($ends.Count); max nearby parked count $maxCount; $first; $res" }
        } }
    )
}
