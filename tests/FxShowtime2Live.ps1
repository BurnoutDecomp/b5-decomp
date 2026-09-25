# FX-SHOWTIME2 live case (crash parity 2026-09-24): the Showtime drive of ShowtimeContacts.ps1 (the
# FxDirectorShowtime / FxCamrigShowtime drive), with the witnesses that the whole showtime per-car score chain runs:
#   ProcessContacts pushes a crashed traffic index      `[showtime-crash] traffic car N crashed ...`  (BRN_SHOWTIME_WATCH)
#   UpdateShowtimeMode pops it into action 116          `[showtime-score] pop traffic car N -> action 116`
#   PhysicsModule::HandleGameActions case 116 runs      `[s3-action] FIRST arrival of game action id 116`
#   TrafficEntityModule::ProcessTrafficTypeRequests     `[td-type] answered index=N ...`              (BRN_TD_DIAG)
#   next frame: DealWithScoreForVehicleClass + 140      `[showtime-score] answer traffic car N ... carsCrashed C ... -> action 140`
#   the director's case 140                             `[director-action] 140 VEHICLE_HIT total ...` (BRN_DIRECTOR_ACTION_DIAG)
#   the GUI translator's case 140                       `[showtime-score] action 140 VEHICLE_HIT -> gui 394 ...` (BRN_SHOWTIME_SCORE_DIAG)
# A missing answer is the console's own assert (BrnGameStateModule.cpp:1611) and prints `[showtime-score] MISS`.
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxShowtime2Live.ps1
# Before (FX-DIRECTOR run scratch/bugtest/runs/fxdirector_showtime/20260924_145256): one `[showtime-crash]` push
# "(no consumer yet -- UpdateShowtimeMode @0x82380EF8 unreconstructed)", no pop, no 116, no answer, no 140, the director
# never saw a VEHICLE_HIT and "Cars Crashed" stayed 0.
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'fxshowtime2_live'
$case.Bug = 'Showtime traffic crashes must be scored: UpdateShowtimeMode pops each crashed traffic car into a traffic-type request (116), the traffic answer comes back the next frame, DealWithScoreForVehicleClass moves Cars Crashed and action 140 reaches the director and the GUI, with no assertions.'
$case.DiagEnv += ',BRN_TD_DIAG=1,BRN_DIRECTOR_ACTION_DIAG=1,BRN_CRASHCAM_DIAG=1,BRN_SHOWTIME_SCORE_DIAG=1,BRN_CRASHPLAY_TRACE=1'
# The bounce half (ProcessGameEvents cases 52 / 53 -> actions 144 / 145): pulse the real boost button through
# Showtime (flow_run -Boost: one press per pulse, the default 1.0 s period is above the console's 0.7 s re-arm
# window), so the car bounces -- each accepted bounce is event 52 -> action 144 -> MainDirector case 144 and
# RaceCarEntityModule -> CrashPlayManager::OnBounce (the [crashplay] `charge` counter). Bouncing also carries
# the car further along the road than the bare launch, i.e. past more traffic. The extra time is for the
# longer Showtime the bounces buy.
# FX-DIRECTOR2 2026-09-25: 50 / 179 (were 41 / 170). The boost still starts 1 s after ShowtimeContacts' gesture,
# which moved 40 -> 49 to clear the crash analyser's 401-update hold on the drive's first wall crash
# (see ShowtimeContacts.ps1).
$case.Run.Boost = '50'
$case.Run.MaxSeconds = 179
# Before (b5 cbe64697 and earlier, e.g. scratch/flow_run/cp3/BrnGame.log 2026-08-29): `[crashplay] ... press=64
# arm=64 charge=0` -- 64 accepted bounce presses and not one reached OnBounce, because no arm relayed event 52.
$case.Checks += @(
    @{ Kind = 'Script'; Name = 'every crashed traffic car pushed is popped into a traffic-type request (action 116)'; Script = {
        param($ctx)
        $pushes = 0; $pops = @()
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[showtime-crash\] traffic car (\d+) crashed') { $pushes++ }
            if ($l -match '\[showtime-score\] pop traffic car (\d+) -> action 116') { $pops += [int]$Matches[1] }
        }
        # The push witness and the pop witness are both capped at 160 lines, so compare below the caps.
        $pass = ($pushes -gt 0) -and ($pops.Count -gt 0) -and ($pops.Count -le $pushes)
        @{ Pass = $pass; Detail = ("{0} pushes, {1} pops ({2})" -f $pushes, $pops.Count, (($pops | Select-Object -First 12) -join ' ')) }
    } }
    @{ Kind = 'LogMatch'; Name = 'PhysicsModule::HandleGameActions received action 116 (the physics hop)';
       Pattern = '\[s3-action\] FIRST arrival of game action id 116 \(size 2\)'; Expect = $true }
    @{ Kind = 'Script'; Name = 'the traffic module answered the popped indices (ProcessTrafficTypeRequests)'; Script = {
        param($ctx)
        $pops = @{}; $answered = 0; $other = 0
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[showtime-score\] pop traffic car (\d+) -> action 116') { $pops[[int]$Matches[1]] = $true; continue }
            if ($l -match '\[td-type\] answered index=(\d+) ') { if ($pops.ContainsKey([int]$Matches[1])) { $answered++ } else { $other++ } }
        }
        # [td-type] is capped at 64 lines and also answers the takedown lane's requests (the "other" count).
        @{ Pass = ($answered -gt 0); Detail = ("{0} [td-type] answers for popped showtime indices, {1} for other requests; {2} distinct pops" -f $answered, $other, $pops.Count) }
    } }
    @{ Kind = 'Script'; Name = 'each request is answered the next frame and scored: DealWithScoreForVehicleClass + action 140, no MISS'; Script = {
        param($ctx)
        $pops = 0; $answers = 0; $miss = 0; $maxCars = 0; $detail = @()
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[showtime-score\] pop traffic car') { $pops++ }
            if ($l -match '\[showtime-score\] MISS') { $miss++ }
            if ($l -match '\[showtime-score\] answer traffic car (\d+) class (\d+) -> DealWithScoreForVehicleClass: carsCrashed (\d+) .* base (\d+) category (\d+) mult \+(\d+) \(total (\d+)\) chain (\d+) -> action 140') {
                $answers++
                $c = [int]$Matches[3]; if ($c -gt $maxCars) { $maxCars = $c }
                if ($detail.Count -lt 6) { $detail += ("car {0} cls {1} base {2} chain {3} -> carsCrashed {4}" -f $Matches[1], $Matches[2], $Matches[4], $Matches[8], $c) }
            }
        }
        # The run can end between a pop and its answer: at most one pop may be unanswered.
        $pass = ($answers -gt 0) -and ($miss -eq 0) -and ($answers -ge ($pops - 1)) -and ($maxCars -gt 0)
        @{ Pass = $pass; Detail = ("{0} pops, {1} answers, {2} MISS; Cars Crashed reached {3}; {4}" -f $pops, $answers, $miss, $maxCars, ($detail -join '; ')) }
    } }
    @{ Kind = 'Script'; Name = 'the director receives every VEHICLE_HIT (case 140), crush/multiplier flags as the console computes them'; Script = {
        param($ctx)
        # The director's case 140 skips a chained hit (rec+0x1C != 0) BEFORE its witness prints, so the expected
        # count is the answers with chain 0; for those, +0x1EA = (total % 10 == 0) and +0x1EB = (mult earned > 0).
        $expect = @(); $seen = @()
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[showtime-score\] answer traffic car \d+ class \d+ -> DealWithScoreForVehicleClass: carsCrashed (\d+) .* mult \+(\d+) \(total \d+\) chain (\d+) -> action 140') {
                if ([int]$Matches[3] -eq 0) {
                    $crush = if (([int]$Matches[1] % 10) -eq 0) { 1 } else { 0 }
                    $mult  = if ([int]$Matches[2] -gt 0) { 1 } else { 0 }
                    $expect += ("{0}/{1}/{2}" -f $Matches[1], $crush, $mult)
                }
            }
            if ($l -match '\[director-action\] 140 VEHICLE_HIT total (\d+) multiplier (-?\d+) -> mbCrushComboThisFrame (\d) mbEarntMultiplierThisFrame (\d)') {
                $seen += ("{0}/{1}/{2}" -f $Matches[1], $Matches[3], $Matches[4])
            }
        }
        $pass = ($expect.Count -gt 0) -and ((@($expect) -join ',') -eq (@($seen) -join ','))
        @{ Pass = $pass; Detail = ("expected (chain-0 hits) total/crush/mult: [{0}]; director printed: [{1}]" -f (($expect | Select-Object -First 10) -join ' '), (($seen | Select-Object -First 10) -join ' ')) }
    } }
    @{ Kind = 'LogMatch'; Name = 'the GUI translator turned 140 into GuiHitVehicleEvent (394)';
       Pattern = '\[showtime-score\] action 140 VEHICLE_HIT -> gui 394'; Expect = $true }
    @{ Kind = 'Script'; Name = 'crash-mode close-ups (reported; one is required only when a chain-0 hit earned a multiplier or hit a tenth car)'; Script = {
        param($ctx)
        $closeupBeats = 0; $beats = 0; $trigger = $false
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[director-action\] 140 VEHICLE_HIT .* mbCrushComboThisFrame (\d) mbEarntMultiplierThisFrame (\d)') {
                if ($Matches[1] -eq '1' -or $Matches[2] -eq '1') { $trigger = $true }
            }
            if ($l -match '\[crashmode\] hb frame \d+ requestedSimScale \S+ impactTimeActive \d impactFactor \S+ closeup (\d)') {
                $beats++; if ($Matches[1] -eq '1') { $closeupBeats++ }
            }
        }
        $pass = (-not $trigger) -or ($closeupBeats -gt 0)
        @{ Pass = $pass; Detail = ("close-up trigger seen: {0}; {1} of {2} [crashmode] heartbeats inside a close-up" -f $trigger, $closeupBeats, $beats) }
    } }
    @{ Kind = 'Script'; Name = 'every showtime bounce (event 52) is relayed as action 144 and reaches the director with the same combo / total'; Script = {
        param($ctx)
        # [showtime-bounce] is capped at 64 lines and [director-action] at 400 shared lines, so compare in order
        # over the shorter list; both must be non-empty.
        $relay = @(); $director = @()
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[showtime-bounce\] event 52 -> action 144: chain (-?\d+) combo (-?\d+) carsCrashed (-?\d+)') {
                $relay += ("{0}/{1}" -f $Matches[2], $Matches[3])
            }
            if ($l -match '\[director-action\] 144 JUST_BOUNCED combo (-?\d+) total (-?\d+)') {
                $director += ("{0}/{1}" -f $Matches[1], $Matches[2])
            }
        }
        $n = [Math]::Min($relay.Count, $director.Count)
        $same = $true
        for ($i = 0; $i -lt $n; $i++) { if ($relay[$i] -ne $director[$i]) { $same = $false } }
        $pass = ($relay.Count -gt 0) -and ($director.Count -gt 0) -and $same
        @{ Pass = $pass; Detail = ("{0} relayed 144s, {1} director 144s, combo/total equal over the first {2}: {3}; first: [{4}]" -f $relay.Count, $director.Count, $n, $same, (($relay | Select-Object -First 8) -join ' ')) }
    } }
    @{ Kind = 'Script'; Name = 'accepted bounce presses are spent by CrashPlayManager::OnBounce (the 144 arm of RaceCarEntityModule)'; Script = {
        param($ctx)
        $last = $null
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[crashplay\] frames=\d+ showtime=(\d+) press=(\d+) arm=(\d+) charge=(\d+)') { $last = @([int]$Matches[1], [int]$Matches[2], [int]$Matches[3], [int]$Matches[4]) }
        }
        if ($null -eq $last) { return @{ Pass = $false; Detail = 'no [crashplay] line (BRN_CRASHPLAY_TRACE never reached the game, or Showtime never ran)' } }
        # A press accepted in the last ~0.3 s of the run can still be in flight, so one arm may be unspent.
        $pass = ($last[2] -gt 0) -and ($last[3] -gt 0)
        @{ Pass = $pass; Detail = ("last [crashplay]: showtime frames {0}, presses {1}, accepted {2}, charged by OnBounce {3}" -f $last[0], $last[1], $last[2], $last[3]) }
    } }
    @{ Kind = 'Script'; Name = 'INFO -- event 53 (the SIXAXIS extra-spin latch) relayed as action 145 (never fails: no PC pad sets the latch)'; Script = {
        param($ctx)
        $relay = @($ctx.LogLines | Where-Object { $_ -match '\[showtime-bounce\] event 53 -> action 145' }).Count
        $director = @($ctx.LogLines | Where-Object { $_ -match '\[director-action\] 145 JUST_APPLIED_EXTRA_SPIN' }).Count
        @{ Pass = $true; Detail = ("{0} relayed 145s, {1} director 145s" -f $relay, $director) }
    } }
)
$case
