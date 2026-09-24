# FX-TRAFFIC3 item 1c (crash parity wave 5, 2026-09-24) -- live witness for the GIVE_UP manoeuvre:
# TrafficEntityModule::UpdateGiveUpManoeuvre @0x8273EB60, GenerateDriverInputs' GIVE_UP arm (0x82749528).
# The organic Road Rage pursuit of RivalOrganic.ps1 shoves traffic about; a physical car that has not
# driven for 10 s (the latch at 0x8274979C) or is stuck at both ends past 3.2 s
# (CheckIfPhysicalVehicleIsStuck) gives up at phase 1, a slammed car still rolling may give up at
# phase 0. On the console the car brakes to a stop, waits 4 s (flt_820BA8DC), waits again while
# touching at both ends (> 0.5 s), and then -- unless the stuck test keeps it in GIVE_UP -- becomes a
# NORMAL physical car (reason 5), indicators off. Witnesses (BRN_TRAFFIC_DIAG, capped):
#   [T-stuck-check] vehicle=V ... -> GIVE_UP phase=1                    (item 1b, a start)
#   [T-give-up] vehicle=V phase=0->1 stopped speed=S                    (phase 0 finished braking)
#   [T-give-up] vehicle=V phase=1 waited=T front=F back=B manoeuvre=M -> NORMAL ... (the hand-back)
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxTraffic3GiveUpLive.ps1 --run-name fxtraffic3_give_up
# (FxTraffic3StuckReverseLive.ps1's wrong-way drive also starts give-ups, through the both-ends arm.)
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxtraffic3_give_up'
$case.Area = 'traffic'
$case.Run.MaxSeconds = 160
$case.Bug = 'A given-up traffic car must run UpdateGiveUpManoeuvre: brake to a stop, wait 4 s, and hand itself back as a NORMAL physical car unless still stuck at both ends (item 1c).'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the GIVE_UP arm is no longer a gate'
       Pattern = 'arm UpdateGiveUpManoeuvre @0x8273EB60 -- no body'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'item 1c dispatched: a given-up car was handed back by UpdateGiveUpManoeuvre'
       Pattern = '\[T-give-up\] vehicle=\d+ phase=1 waited='; Min = 1 }
    @{ Kind = 'Script'; Name = 'every hand-back waited the console 4 s and was not stuck at both ends (> 0.5)'; Script = {
        param($ctx)
        $inv = [cultureinfo]::InvariantCulture
        $n = 0; $bad = @(); $starts = 0; $stops = 0; $retaken = 0
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[T-stuck-check\] vehicle=\d+ .*-> GIVE_UP') { $starts++ }
            elseif ($line -match '\[T-give-up\] vehicle=\d+ phase=0->1 stopped speed=([^ ]+)') {
                $stops++
                if ([math]::Abs([double]::Parse($Matches[1], $inv)) -gt 1.0000001) { $bad += $line }
            }
            elseif ($line -match '\[T-give-up\] vehicle=\d+ phase=1 waited=([^ ]+) front=([^ ]+) back=([^ ]+) manoeuvre=(\d+) -> NORMAL') {
                $n++
                $t = [double]::Parse($Matches[1], $inv); $f = [double]::Parse($Matches[2], $inv); $b = [double]::Parse($Matches[3], $inv)
                if ([int]$Matches[4] -ne 3) { $retaken++ }   # the stuck test took it out of GIVE_UP (NONE / STUCK_REVERSE)
                if ($t -lt 4.0 -or ($f -gt 0.5000001 -and $b -gt 0.5000001)) { $bad += $line }
            }
        }
        @{ Pass = ($n -gt 0 -and $bad.Count -eq 0); Detail = "$starts both-ends GIVE_UP starts, $stops phase-0 stops, $n hand-backs ($retaken after the stuck test changed the manoeuvre); off-rule: $($bad.Count) $(($bad | Select-Object -First 1))" }
    } }
)
$case
