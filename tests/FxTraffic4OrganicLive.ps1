# FX-TRAFFIC4 items 2 + 3 (crash parity wave 5, 2026-09-24) -- live witnesses on an ORGANIC traffic run
# driven by tests/run_rival_organic.py (the rival-pursuit event of tests/RivalOrganic.ps1: pad input only,
# nothing injected), with BRN_TRAFFIC_DIAG on.
#   item 3: DriveTowardsTarget @0x8273DFC0 steers each driving physical car through
#           CalculateAndSetSteeringUsingAvoidance @0x8273D258. [T-avoid] (capped) is that call: the risk
#           Avoidance_GetBestVehicleDirection @0x8272C248 reports, and whether the steering direction
#           moved to the best feeler ("blend" / "snap") or kept the target ("target"). [T-avoid-handbrake]
#           is DriveTowardsTarget's handbrake leg (risk >= 0.7 -> mfHandBrake 0.5, 0x8273E744).
#   item 2: [T-3pt-turn] start / phase / done is Update3PointTurnManoeuvre @0x827190B0 (INFO only: a
#           three-point turn needs a driving car whose target falls 15 m behind it).
# RED on a pre-fix exe: no [T-avoid] line at all, and the one-shot gate line
# "CalculateAndSetSteeringUsingAvoidance @0x8273D258 -- unreconstructed" instead.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxTraffic4OrganicLive.ps1 --run-name fxtraffic4
@{
    Name = 'fxtraffic4_organic'
    Area = 'traffic'
    Bug  = 'DriveTowardsTarget never steered through CalculateAndSetSteeringUsingAvoidance (a gate) and never pulled the avoidance handbrake; the 3-point turn had no body (FX-TRAFFIC4 items 2 + 3).'
    Frames = $false
    Run = @{
        Drive = $true
        Boost = '6:3:2'
        MotionProbe = $true
        SkipIntro = $true
        AcceptGap = 1.0
        Teleport = '3040.7,-5.8,-1937.9,180'
        StartEvent = $true
        EventFsm = $true
        SkipTrainingTip = $true
        MaxSeconds = 100
    }
    DiagEnv = 'BRN_TRAFFIC_DIAG=1,BRN_TRAFFIC_TRACK=1,BRN_RIVAL_PURSUIT_DIAG=1'
    Checks = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
        @{ Kind = 'LogCount'; Name = 'the avoidance gate is gone'; Pattern = 'CalculateAndSetSteeringUsingAvoidance @0x8273D258 -- unreconstructed'; Max = 0 }
        @{ Kind = 'Script'; Name = 'a driving physical car steered through the avoidance call ([T-avoid])'; Script = {
            param($ctx)
            $n = 0; $cars = @{}
            foreach ($line in $ctx.LogLines) {
                if ($line -match '\[T-avoid\] vehicle=(\d+) ') { $n++; $cars[[int]$Matches[1]] = $true }
            }
            @{ Pass = ($n -gt 0); Detail = "$n [T-avoid] lines, cars [$(($cars.Keys | Sort-Object) -join ',')]" }
        } }
        @{ Kind = 'Script'; Name = 'the avoidance ENGAGED: risk >= 0.2 and the direction moved to a feeler (blend / snap)'; Script = {
            param($ctx)
            $inv = [cultureinfo]::InvariantCulture
            $engaged = @(); $maxRisk = 0.0
            foreach ($line in $ctx.LogLines) {
                if ($line -match '\[T-avoid\] vehicle=(\d+) risk=([^ ]+) dist=([^ ]+) packets=(\d+) mode=(\w+) dotAvoidTarget=([^ ]+)') {
                    $r = 0.0
                    if ([double]::TryParse($Matches[2], [Globalization.NumberStyles]::Float, $inv, [ref]$r) -and $r -gt $maxRisk) { $maxRisk = $r }
                    if ($Matches[5] -eq 'blend' -or $Matches[5] -eq 'snap') {
                        $engaged += "v$($Matches[1]) risk=$($Matches[2]) $($Matches[5]) dot=$($Matches[6]) packets=$($Matches[4])"
                    }
                }
            }
            @{ Pass = ($engaged.Count -gt 0); Detail = "max risk $([math]::Round($maxRisk, 3)); engaged: $(($engaged | Select-Object -First 6) -join '; ')" }
        } }
        @{ Kind = 'Script'; Name = 'INFO -- the avoidance handbrake and the three-point turn (never fails)'; Script = {
            param($ctx)
            $hb = @($ctx.LogLines | Where-Object { $_ -match '\[T-avoid-handbrake\]' })
            $turn = @($ctx.LogLines | Where-Object { $_ -match '\[T-3pt-turn\]' })
            $first = @($hb | Select-Object -First 2) + @($turn | Select-Object -First 4)
            @{ Pass = $true; Detail = "$($hb.Count) handbrake pulls, $($turn.Count) 3-pt-turn lines. $(($first | ForEach-Object { $_.Trim() }) -join ' | ')" }
        } }
    )
}
