# FX-SCENARIOS (crash parity 2026-09-25) -- live witness that an offline RACE still gets the console's intro
# length after GameMode::GetIntroDurationSeconds @0x82315A88 lost its 6.0 s stub (b5 ef8b4c58), and still
# reaches E_GMS_IN_PROGRESS.
# The console base returns flt_820211C8 = 6.0 for every offline mode that inherits it (RaceMode's vtable
# 0x820D0498 slot 8 IS the base): the CURRENT mode's online byte (GameMode +0xAC) is 0, so the flyby arm is never taken.
# Drive: the offline race at junction 480886 (FxFlowFinishLine / FxDirectorCheckpointLive's start), started by
# -StartEvent, throttle held -- the intro only has to run and hand over; nobody has to win.
# Witnesses:
#   [mode-intro] IntroState::OnEnter mode type T 'Name' countdown S s timed B   (BRN_INTRO_TIMER_DIAG, NOT X360)
#   [stunt] mode state -> E_GMS_IN_PROGRESS                                     (the always-on mode-state rung)
# First GREEN: fxscenarios_mode_intro_race/20260925_105848 (exe 1c59049819d0, 5/5: countdown 6.000000, IN_PROGRESS 1546 lines later).
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxScenariosModeIntroRaceLive.ps1
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxscenarios_mode_intro_race'
$case.Area = 'mode'
$case.Bug = 'An offline race must keep the console intro length (slot 8 -> 6.0 for mode type 0) and still reach E_GMS_IN_PROGRESS after the base GetIntroDurationSeconds stub was replaced by the console body.'
$case.Run.Teleport = '3003.9,6.6,-1675.6,0'
$case.Run.Boost = ''
$case.Run.MaxSeconds = 75
$case.DiagEnv = 'BRN_INTRO_TIMER_DIAG=1,BRN_PROP_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'Script'; Name = 'the race intro is the console length for its mode (type 0 -> 6.0, flt_820211C8)'; Script = {
        param($ctx)
        $inv = [cultureinfo]::InvariantCulture
        $rows = @($ctx.LogLines | Where-Object { $_ -match '\[mode-intro\] IntroState::OnEnter mode type (\d+) ' })
        $race = @($rows | Where-Object { $_ -match 'mode type 0 ' })
        $bad = @($race | Where-Object { -not ($_ -match "countdown ([-+0-9.eE]+) s timed (\d)" -and [double]::Parse($Matches[1], $inv) -eq 6.0) })
        @{ Pass = ($race.Count -gt 0 -and $bad.Count -eq 0); Detail = "$($rows.Count) [mode-intro] line(s), $($race.Count) for mode type 0, off the console 6.0: $($bad.Count). $(($rows | Select-Object -First 3 | ForEach-Object { $_.Trim() }) -join ' | ')" }
    } }
    @{ Kind = 'Script'; Name = 'the race still reaches E_GMS_IN_PROGRESS after its intro'; Script = {
        param($ctx)
        $intro = -1; $prog = -1
        for ($i = 0; $i -lt $ctx.LogLines.Count; $i++) {
            $l = $ctx.LogLines[$i]
            if ($intro -lt 0 -and $l -match '\[mode-intro\] IntroState::OnEnter mode type 0 ') { $intro = $i }
            if ($intro -ge 0 -and $l -match '\[stunt\] mode state -> E_GMS_IN_PROGRESS') { $prog = $i; break }
        }
        @{ Pass = ($intro -ge 0 -and $prog -gt $intro); Detail = "race intro at log line $intro; IN_PROGRESS at log line $prog" }
    } }
)
$case
