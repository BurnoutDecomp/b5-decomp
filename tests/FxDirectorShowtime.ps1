# FX-DIRECTOR live case (crash parity 2026-09-24): the Showtime drive of ShowtimeContacts.ps1, with the witness that the
# director's Showtime arms -- MainDirector::ProcessInputQueue @0x822372F8 cases 42/43/140/144/145/146 -- reach the
# GameState ArbStateCrashMode reads, so crash mode requests the impact-time factor instead of 0 and the sim no longer
# crawls at MainDirector::Update's 0.005 floor.
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxDirectorShowtime.ps1
# Diagnostics only print; the drive, the Showtime gesture and the inherited checks are unchanged.
#   BRN_DIRECTOR_ACTION_DIAG `[director-action] <id> ... -> <GameState values>` per landed arm (capped), and the crash-mode
#                            heartbeat `[crashmode] hb frame N requestedSimScale S impactTimeActive A impactFactor F
#                            closeup C super U closeupTime T comboLevel L` (ArbStateCrashMode::TickActive, every 30th frame).
#   BRN_CRASHCAM_DIAG        the arbitrator's state edges (`container current state -> 4 (ArbStateCrashMode)`).
#   BRN_CAMRIG_DIAG          `[camrig] Update frame N ... car x y z ...` -- the car origin the crash-mode rig follows,
#                            used here to measure how far the car moves while crash mode owns the frame.
#   BRN_SLOMO_DIAG           (inherited) the SIM timer's live scale, printed when it moves.
# Before (FX-CAMRIG run scratch/bugtest/runs/fxcamrig_showtime/20260924_130456): `[bounce] POSTED game action 42 ... duration
# 1.000000`, then `[slomo] camera post-arbitrator mfSimTimeScale=0.000000` -> `[slomo] simScale=0.005000` for the whole of
# crash mode, the rig's car moving 0.3 m.
# FX-SCENARIOS (2026-09-25): the rig-travel check failed on the case's own first-boot path in 3 of 3 runs because
# crash mode took the frame 6.0 s after the Showtime started (LIVE_FINAL's "720 frames" = 720 [motion] n), with
# the car at rest. That was a PC defect, not the drive: CrashMode had no slot-8 GetIntroDurationSeconds (console
# 0x827E2600 = flt_82008718 = 0.0002 s) and inherited the base's 6.0 s stub, so IntroState held the mode 6.0 s in
# E_GMS_INTRO. Fixed in b5 78d88608; first GREEN on the fix: fxdirector_showtime/20260925_103605 (crash mode on
# the first mode update, rig travel 15.92 m). The two checks at the end pin it: the Showtime's [mode-intro] line
# (BRN_INTRO_TIMER_DIAG, NOT X360) carries the console's 0.0002 s, and crash mode takes the frame before one
# second of mode time has passed (re-scored: 081013 FAIL at tMode 6.000023, 103605 PASS at 0.016667; first run
# with them: fxdirector_showtime/20260925_111205 GREEN 15/15).
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'fxdirector_showtime'
$case.Bug = 'Showtime crash mode must request the impact-time factor the director received (action 42), not 0: no 0.005 sim floor, the car moves, no assertions.'
$case.DiagEnv += ',BRN_DIRECTOR_ACTION_DIAG=1,BRN_CRASHCAM_DIAG=1,BRN_CAMRIG_DIAG=1,BRN_INTRO_TIMER_DIAG=1'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'director arm 42 stored the posted impact-time factor (1.0)';
       Pattern = '\[director-action\] 42 IMPACT_TIME_START -> mbImpactTimeActive 1 mfImpactTimeSloMoFactor 1\b'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'director handed the frame to ArbStateCrashMode';
       Pattern = '\[crashcam\] container current state -> 4 \(ArbStateCrashMode\)'; Expect = $true }
    @{ Kind = 'Script'; Name = 'crash mode requests the impact-time factor (not 0) while no close-up runs'; Script = {
        param($ctx)
        $inv = [Globalization.CultureInfo]::InvariantCulture
        $num = { param($s) $v = 0.0; if ([double]::TryParse($s, [Globalization.NumberStyles]::Float, $inv, [ref]$v)) { $v } else { [double]::NaN } }
        $rx = '\[crashmode\] hb frame (\d+) requestedSimScale (\S+) impactTimeActive (\d) impactFactor (\S+) closeup (\d) super (\d)'
        $rows = 0; $bad = 0; $closeups = 0; $scales = @()
        foreach ($l in $ctx.LogLines) {
            if ($l -match $rx) {
                $rows++
                $scale = & $num $Matches[2]; $factor = & $num $Matches[4]
                if ($Matches[5] -eq '1') { $closeups++; continue }
                $scales += $scale
                if (-not ($scale -eq $factor -and $scale -gt 0.005)) { $bad++ }
            }
        }
        if ($rows -lt 3) { return @{ Pass = $false; Detail = "$rows [crashmode] heartbeat lines (want >= 3)" } }
        $sorted = @($scales | Sort-Object)
        $lo = if ($sorted.Count) { $sorted[0] } else { [double]::NaN }; $hi = if ($sorted.Count) { $sorted[-1] } else { [double]::NaN }
        @{ Pass = ($bad -eq 0 -and $scales.Count -gt 0);
           Detail = ("{0} heartbeat lines, {1} outside a close-up, {2} not equal to the factor or <= 0.005; requested scale min {3} max {4}; {5} lines inside a close-up" -f
                     $rows, $scales.Count, $bad, $lo, $hi, $closeups) }
    } }
    @{ Kind = 'Script'; Name = 'the SIM never drops to the 0.005 floor once crash mode owns the frame'; Script = {
        param($ctx)
        $entered = $false; $floor = 0; $scaleLines = @(); $beats = 0
        foreach ($l in $ctx.LogLines) {
            if (-not $entered -and $l -match '\[crashcam\] container current state -> 4 \(ArbStateCrashMode\)') { $entered = $true; continue }
            if (-not $entered) { continue }
            if ($l -match '\[crashmode\] hb frame') { $beats++ }
            if ($l -match '\[slomo\] simScale=(\S+)') { $scaleLines += $Matches[1]; if ($Matches[1] -eq '0.005000') { $floor++ } }
        }
        if (-not $entered) { return @{ Pass = $false; Detail = 'crash mode never entered' } }
        # A positive witness is required too: the heartbeat proves crash mode ran while no floor line appeared.
        @{ Pass = ($floor -eq 0 -and $beats -gt 0);
           Detail = ("after crash-mode entry: {0} [slomo] simScale lines ({1}), {2} at 0.005; {3} [crashmode] heartbeats" -f
                     $scaleLines.Count, (($scaleLines | Select-Object -First 6) -join ' '), $floor, $beats) }
    } }
    @{ Kind = 'Script'; Name = 'the car moves while crash mode owns the frame (rig car origin travel)'; Script = {
        param($ctx)
        $inv = [Globalization.CultureInfo]::InvariantCulture
        $num = { param($s) $v = 0.0; if ([double]::TryParse($s, [Globalization.NumberStyles]::Float, $inv, [ref]$v)) { $v } else { [double]::NaN } }
        $rx = '\[camrig\] Update frame (\d+) first (\d) tempDebug (\d) manual (\d) car (\S+) (\S+) (\S+) cam '
        $rows = @()
        foreach ($l in $ctx.LogLines) {
            if ($l -match $rx) { $rows += ,@((& $num $Matches[5]), (& $num $Matches[6]), (& $num $Matches[7])) }
        }
        if ($rows.Count -lt 10) { return @{ Pass = $false; Detail = "$($rows.Count) [camrig] lines (want >= 10)" } }
        $c0 = $rows[0]; $max = 0.0; $path = 0.0; $prev = $c0
        foreach ($r in $rows) {
            $d = [math]::Sqrt(($r[0]-$c0[0])*($r[0]-$c0[0]) + ($r[1]-$c0[1])*($r[1]-$c0[1]) + ($r[2]-$c0[2])*($r[2]-$c0[2]))
            if ($d -gt $max) { $max = $d }
            $path += [math]::Sqrt(($r[0]-$prev[0])*($r[0]-$prev[0]) + ($r[1]-$prev[1])*($r[1]-$prev[1]) + ($r[2]-$prev[2])*($r[2]-$prev[2]))
            $prev = $r
        }
        # Before the fix the same drive moved the rig's car 0.3 m in 125 rig frames (0.005x sim).
        @{ Pass = ($max -gt 2.0); Detail = ("{0} rig lines: car max displacement {1:f2} m, sampled path {2:f2} m (before: 0.3 m)" -f $rows.Count, $max, $path) }
    } }
    @{ Kind = 'Script'; Name = 'director Showtime arms dispatched (counts per action id)'; Script = {
        param($ctx)
        $counts = @{}
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[director-action\] (\d+) ') { $k = $Matches[1]; if ($counts.ContainsKey($k)) { $counts[$k]++ } else { $counts[$k] = 1 } }
        }
        $parts = @($counts.Keys | Sort-Object { [int]$_ } | ForEach-Object { "$_ x$($counts[$_])" })
        @{ Pass = $counts.ContainsKey('42'); Detail = ("[director-action] ids: {0}" -f ($parts -join ', ')) }
    } }
    @{ Kind = 'Script'; Name = 'the Showtime intro is the console slot 8 of CrashMode (0x827E2600: flt_82008718 = 0.0002 s, timed)'; Script = {
        param($ctx)
        $inv = [cultureinfo]::InvariantCulture
        $rows = @($ctx.LogLines | Where-Object { $_ -match '\[mode-intro\] IntroState::OnEnter mode type 2 ' })
        $good = @($rows | Where-Object { $_ -match "countdown ([-+0-9.eE]+) s timed 1" -and [math]::Abs([double]::Parse($Matches[1], $inv) - 0.0002) -lt 5e-7 })
        @{ Pass = ($rows.Count -gt 0 -and $good.Count -eq $rows.Count); Detail = "$($rows.Count) Showtime [mode-intro] line(s), $($good.Count) at 0.0002 s timed: $(($rows | Select-Object -First 1 | ForEach-Object { $_.Trim() }))" }
    } }
    @{ Kind = 'Script'; Name = 'crash mode takes the frame within one second of mode time of the Showtime intro (was 6.0 s)'; Script = {
        param($ctx)
        $inv = [cultureinfo]::InvariantCulture
        $intro = -1; $tMode = 0.0; $entered = -1
        for ($i = 0; $i -lt $ctx.LogLines.Count; $i++) {
            $l = $ctx.LogLines[$i]
            if ($intro -lt 0) {
                if ($l -match '\[director-action\] 146 SHOWTIME_INTRO_START') { $intro = $i }
                continue
            }
            if ($l -match '\[crash-end\] poll=\d+ .* tMode=([-+0-9.eE]+)') { $tMode = [double]::Parse($Matches[1], $inv) }
            if ($l -match '\[crashcam\] container current state -> 4 \(ArbStateCrashMode\)') { $entered = $i; break }
        }
        @{ Pass = ($intro -ge 0 -and $entered -gt $intro -and $tMode -lt 1.0)
           Detail = ("Showtime intro at log line {0}; crash mode at log line {1}; last [crash-end] tMode before it {2:f6} s" -f $intro, $entered, $tMode) }
    } }
)
$case
