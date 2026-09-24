# FX-TAILS-A item 4 live case (crash parity 2026-09-24): CrashPlayManager's Showtime meter arithmetic (b5 46304074)
# is dispatched in a real Showtime -- OnBounce's charged arm @0x822A7EF8 (the fused bounce cost, fmadds @0x822A7F6C)
# and OnVehicleHitConfirmed @0x822C3348 (the fused per-vehicle award, fmadds @0x822C33AC; the every-N award
# @0x822C3448 needs a tenth car, reported here as INFO). The numeric proof is run_fxtailsa_crash_play.py; this case
# proves both bodies RAN on real game input and the award they paid is the one each scored base score earns.
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxTailsACrashPlayLive.ps1
# The drive is ShowtimeContacts.ps1's (fresh profile, both-bumpers Showtime entry at 40 s) with the boost button
# pulsed through Showtime (flow_run -Boost, as FxShowtime2Live.ps1 does) so the car bounces and OnBounce charges.
# Witnesses (BRN_CRASHPLAY_TRACE, the [crashplay] line every 30 CrashPlayManager::Update frames):
#   charge / chargeSpent   OnBounce's charged arm ran and spent the fused cost
#   hits / paid / hitAward OnVehicleHitConfirmed ran (action 140 reached the manager) / unchained hits paid / their sum
# against the GameState's own answers (BRN_SHOWTIME_WATCH): `[showtime-score] answer ... base B ... chain C -> action 140`.
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'fxtailsa_crash_play'
$case.Bug = 'CrashPlayManager::OnBounce and OnVehicleHitConfirmed must run in a live Showtime with the console''s arithmetic: every bounce charge and every scored traffic hit reaches the manager, and each unchained hit pays 20 + 15 * (clamp(base, 1000, 5000) - 1000) / 4000 (0x82CDB508 / 0x82CDB50C / 0x82CDB55C / 0x82CDB560), with no assertions.'
$case.DiagEnv += ',BRN_CRASHPLAY_TRACE=1'
$case.Run.Boost = '41'
$case.Run.MaxSeconds = 170
$case.Checks += @(
    @{ Kind = 'Script'; Name = 'OnBounce''s charged arm ran (the fused bounce cost was spent)'; Script = {
        param($ctx)
        $last = $null
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[crashplay\] frames=\d+ showtime=(\d+) press=(\d+) arm=(\d+) charge=(\d+) .* chargeSpent=(\S+)') {
                $last = @([int]$Matches[1], [int]$Matches[2], [int]$Matches[3], [int]$Matches[4], [double]::Parse($Matches[5], [Globalization.CultureInfo]::InvariantCulture))
            }
        }
        if ($null -eq $last) { return @{ Pass = $false; Detail = 'no [crashplay] line (BRN_CRASHPLAY_TRACE never reached the game, or Showtime never ran)' } }
        # Each charge costs 10 + 10 * difficulty (0x82CDB528 / 0x82CDB52C), difficulty in [0, 1].
        $pass = ($last[3] -gt 0) -and ($last[4] -ge 10.0 * $last[3] - 0.01) -and ($last[4] -le 20.0 * $last[3] + 0.01)
        @{ Pass = $pass; Detail = ("last [crashplay]: showtime frames {0}, presses {1}, accepted {2}, charged {3}, spent {4:0.000000} (want within [10, 20] per charge)" -f $last[0], $last[1], $last[2], $last[3], $last[4]) }
    } }
    @{ Kind = 'Script'; Name = 'OnVehicleHitConfirmed ran for every scored hit and paid each unchained hit its console award'; Script = {
        param($ctx)
        $answers = 0; $chain0 = @(); $w = $null
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[showtime-score\] answer traffic car \d+ class \d+ -> DealWithScoreForVehicleClass: carsCrashed \d+ .* base (-?\d+) category \d+ mult \+-?\d+ \(total -?\d+\) chain (-?\d+) -> action 140') {
                $answers++
                if ([int]$Matches[2] -eq 0) { $chain0 += [int]$Matches[1] }
                continue
            }
            if ($l -match '\[crashplay\] frames=.* \| hits=(\d+) paid=(\d+) everyN=(\d+) hitAward=(\S+)') {
                $w = @{ Hits = [int]$Matches[1]; Paid = [int]$Matches[2]; EveryN = [int]$Matches[3];
                        Award = [double]::Parse($Matches[4], [Globalization.CultureInfo]::InvariantCulture);
                        Answers = $answers; Chain0 = $chain0.Count; Bases = @($chain0) }
            }
        }
        if ($null -eq $w) { return @{ Pass = $false; Detail = 'no [crashplay] line with the hits= fields (exe older than the witness, or Showtime never ran)' } }
        # At that witness line, at most one answered hit can still be in the action queue.
        $expect = 0.0
        foreach ($b in ($w.Bases | Select-Object -First $w.Paid)) {
            $c = [Math]::Min([Math]::Max([double]$b, 1000.0), 5000.0)
            $expect += 20.0 + 15.0 * (($c - 1000.0) / 4000.0)
        }
        $pass = ($w.Hits -gt 0) -and ($w.Paid -gt 0) -and
                ($w.Hits -le $w.Answers) -and ($w.Hits -ge $w.Answers - 1) -and
                ($w.Paid -le $w.Chain0) -and ($w.Paid -ge $w.Chain0 - 1) -and
                ([Math]::Abs($w.Award - $expect) -le 0.01)
        @{ Pass = $pass; Detail = ("at the last [crashplay] line: {0} scored answers ({1} unchained, bases {2}) -> OnVehicleHitConfirmed hits {3}, paid {4}, award {5:0.000000} (expected {6:0.000000}), every-N awards {7}" -f $w.Answers, $w.Chain0, (($w.Bases | Select-Object -First 8) -join ' '), $w.Hits, $w.Paid, $w.Award, $expect, $w.EveryN) }
    } }
    @{ Kind = 'Script'; Name = 'the meter stayed in [0, 100] (both clamps ran)'; Script = {
        param($ctx)
        $bad = 0; $n = 0; $min = 1000.0; $max = -1000.0
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[crashplay\] frames=.* \| boost=(\S+) ') {
                $v = 0.0; $n++
                if (-not [double]::TryParse($Matches[1], [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$v)) { $bad++; continue }
                if ($v -lt $min) { $min = $v }; if ($v -gt $max) { $max = $v }
                if ($v -lt 0.0 -or $v -gt 100.0 -or [double]::IsNaN($v)) { $bad++ }
            }
        }
        @{ Pass = ($n -gt 0) -and ($bad -eq 0); Detail = ("{0} [crashplay] samples, boost in [{1}, {2}], {3} outside [0, 100]" -f $n, $min, $max, $bad) }
    } }
)
$case
