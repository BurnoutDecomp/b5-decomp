# FX-RCEM (crash-parity 2026-09-22): the two reset-on-track speeds that were pinned to 0.0.
#   * ProcessRaceCarCrashCompleteEvents (ARTIST 0x822F4440..0x822F4478): a wreck handed back with
#     its engine running (meEngineState == 2) is reset at flt_82FAD720 = 22.352 m/s (50 mph)
#     offline / flt_82FAD8C0 = 33.528 m/s online; otherwise at 0.0.
#   * ProcessResetOnTrackResultQueue's FAILURE arm (0x822F46C4..0x822F46DC): min(requested,
#     flt_82FAD610 = 4.4704 m/s, 10 mph).
# Both words are BSS written by CRT initialisers (0x82C4BB30 / 0x82C4BB50 / 0x82C4BB10).
# Built on tools/tests/cases/takedown_forced.ps1: a Road Rage event where rivals crash and are
# reset, so both paths run organically after the forced takedown.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/RcemResetSpeeds.ps1
$workflow = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$case = & (Join-Path $workflow 'tools/tests/cases/takedown_forced.ps1')
$case.Name = 'rcem_reset_speeds'
$case.Bug = 'Crash-exit resets with a running engine roll at 50 mph; FAILURE-arm resets roll at up to 10 mph.'
$case.Checks += @(
    @{ Kind = 'Script'; Name = 'crash-exit reset speed follows the engine state'; Script = {
        param($ctx)
        $bad = @(); $running = 0; $other = 0
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[crash-exit\] reset-on-track active race car \d+ engineState (\d+) online (\d) speed ([-+0-9.eE]+)') {
                $state = [int]$Matches[1]; $online = [int]$Matches[2]
                $speed = [double]::Parse($Matches[3], [cultureinfo]::InvariantCulture)
                $want = if ($state -eq 2) { if ($online -eq 1) { 33.528 } else { 22.352 } } else { 0.0 }
                if ($state -eq 2) { $running++ } else { $other++ }
                if ([math]::Abs($speed - $want) -gt 0.001) { $bad += $line }
            }
        }
        @{ Pass = ($bad.Count -eq 0 -and ($running + $other) -gt 0);
           Detail = "$running running-engine and $other other crash-exit resets; mismatches: $($bad.Count) $(($bad | Select-Object -First 1))" }
    } }
    @{ Kind = 'Script'; Name = 'FAILURE-arm resets are capped at 10 mph, not at rest'; Script = {
        param($ctx)
        $pending = @{}; $bad = @(); $seen = 0
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[resetpump\] request SENT: global car (\d+) type \d+ speed ([-+0-9.eE]+)') {
                $pending[$Matches[1]] = [double]::Parse($Matches[2], [cultureinfo]::InvariantCulture)
            } elseif ($line -match '\[resetpump\] RESULT applied: global car (\d+) FAILURE .* speed ([-+0-9.eE]+)') {
                if ($pending.ContainsKey($Matches[1])) {
                    $seen++
                    $applied = [double]::Parse($Matches[2], [cultureinfo]::InvariantCulture)
                    $want = [math]::Min($pending[$Matches[1]], 4.4704)
                    if ([math]::Abs($applied - $want) -gt 0.001) { $bad += $line }
                }
            }
        }
        @{ Pass = ($seen -gt 0 -and $bad.Count -eq 0);
           Detail = "$seen FAILURE results checked against min(requested, 4.4704); mismatches: $($bad.Count) $(($bad | Select-Object -First 1))" }
    } }
)
$case
