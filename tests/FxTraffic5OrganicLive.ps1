# FX-TRAFFIC5 (crash parity wave 5, 2026-09-24) -- live witnesses on an ORGANIC traffic run driven by
# tests/run_rival_organic.py (the rival-pursuit event of tests/RivalOrganic.ps1: pad input only, nothing
# injected), with BRN_TRAFFIC_DIAG on. The first links of the traffic hit-reaction chain:
#   UpdateCollidableVehicles @0x827302C8 -- [T-collidable] (6 consecutive frames of every 120, capped):
#     the avoidance cache's packet count, the traffic cars cached, and `carried` = the cars of the half of
#     the pool NOT re-evaluated this frame that stayed cached through mVehiclesAvoidableLastFrame
#     (0x82731A54..0x82731D18). Pre-fix that leg did not exist: carried was always 0 and the cache
#     alternated between the two halves.
#   AddVehicleToPhysics @0x827425B0 -- [T-createq] (capped): each new high-water mark of the create
#     queue's fill at the console's queue-full check (0x82742674), and any refused post (FULL).
#   DriveTowardsTarget @0x8273DFC0 -- its NaN-only legs are unobservable live; [T-avoid] shows the
#     driving path it feeds still runs.
# RED on a pre-fix exe: no [T-collidable] / [T-createq] line at all.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxTraffic5OrganicLive.ps1 --run-name fxtraffic5
@{
    Name = 'fxtraffic5_organic'
    Area = 'traffic'
    Bug  = 'UpdateCollidableVehicles dropped the avoidable carry-over for the un-evaluated half of the pool (the avoidance cache alternated), gated the road-following camera source and added an invented sweep; AddVehicleToPhysics had no queue-full leg (FX-TRAFFIC5).'
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
        @{ Kind = 'Script'; Name = 'the carry-over leg ran: [T-collidable] with carried > 0 (0x82731A54..0x82731D18)'; Script = {
            param($ctx)
            $n = 0; $carriedFrames = 0; $maxCarried = 0; $samples = @()
            foreach ($line in $ctx.LogLines) {
                if ($line -match '\[T-collidable\] frame=(\d+) packets=(\d+) cachedCars=(\d+) carried=(\d+)') {
                    $n++
                    $c = [int]$Matches[4]
                    if ($c -gt 0) { $carriedFrames++ }
                    if ($c -gt $maxCarried) { $maxCarried = $c }
                    if ($samples.Count -lt 12) { $samples += "f$($Matches[1]):p$($Matches[2])/c$($Matches[3])/k$c" }
                }
            }
            @{ Pass = ($carriedFrames -gt 0); Detail = "$n lines, $carriedFrames with carried > 0, max carried $maxCarried; $($samples -join ' ')" }
        } }
        @{ Kind = 'Script'; Name = 'the cache holds both halves: no frame-to-frame 2x packet swing inside a sampled window'; Script = {
            param($ctx)
            $prevFrame = -10; $prev = -1; $swings = 0; $pairs = 0; $worst = ''
            foreach ($line in $ctx.LogLines) {
                if ($line -match '\[T-collidable\] frame=(\d+) packets=(\d+) cachedCars=(\d+) carried=(\d+)') {
                    $f = [int]$Matches[1]; $p = [int]$Matches[2]
                    if ($f -eq $prevFrame + 1 -and $prev -ge 0) {
                        $pairs++
                        $lo = [math]::Min($p, $prev); $hi = [math]::Max($p, $prev)
                        if ($hi -ge 4 -and $hi -ge 2 * [math]::Max($lo, 1)) { $swings++; if ($worst -eq '') { $worst = "f$($f-1)->f${f}: $prev->$p" } }
                    }
                    $prevFrame = $f; $prev = $p
                }
            }
            @{ Pass = ($pairs -gt 0 -and $swings -eq 0); Detail = "$pairs consecutive-frame pairs, $swings halving swings $worst" }
        } }
        @{ Kind = 'Script'; Name = 'the create-queue check ran: [T-createq] (0x82742674)'; Script = {
            param($ctx)
            $lines = @($ctx.LogLines | Where-Object { $_ -match '\[T-createq\]' })
            $full = @($lines | Where-Object { $_ -match 'FULL' })
            $max = 0
            foreach ($line in $lines) { if ($line -match 'queued=(\d+)/') { if ([int]$Matches[1] -gt $max) { $max = [int]$Matches[1] } } }
            @{ Pass = ($lines.Count -gt 0); Detail = "$($lines.Count) lines, high-water $max/25, $($full.Count) refused (FULL). $(($lines | Select-Object -First 3 | ForEach-Object { $_.Trim() }) -join ' | ')" }
        } }
        @{ Kind = 'Script'; Name = 'INFO -- the hit chain and the driving path (never fails)'; Script = {
            param($ctx)
            $avoid = @($ctx.LogLines | Where-Object { $_ -match '\[T-avoid\] vehicle=' })
            $arm = @($ctx.LogLines | Where-Object { $_ -match '\[T5-arm\]' })
            $hit = @($ctx.LogLines | Where-Object { $_ -match '\[T4-hit\] outcome=' })
            @{ Pass = $true; Detail = "$($avoid.Count) [T-avoid], $($arm.Count) [T5-arm], $($hit.Count) [T4-hit] outcomes. $(($hit | Select-Object -First 3 | ForEach-Object { $_.Trim() }) -join ' | ')" }
        } }
    )
}
