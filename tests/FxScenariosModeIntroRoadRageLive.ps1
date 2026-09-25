# FX-SCENARIOS (crash parity 2026-09-25) -- live witness that an offline ROAD RAGE still gets the console's intro
# length after GameMode::GetIntroDurationSeconds @0x82315A88 lost its 6.0 s stub (b5 ef8b4c58), and still
# reaches E_GMS_IN_PROGRESS. The second event type the conductor asked for beside the race
# (FxScenariosModeIntroRaceLive.ps1).
# RoadRageMode's vtable 0x820D05E8 slot 8 is the base 0x82315A88 (x360rd), which returns flt_820211C8 = 6.0
# for an offline mode (the current mode's online byte, GameMode +0xAC, is 0).
# Drive: RivalOrganic.ps1's Road Rage start (its own teleport + -StartEvent), throttle held, no steering driver.
# Witnesses as in the race variant: [mode-intro] (BRN_INTRO_TIMER_DIAG, NOT X360) and the [stunt] mode-state rung.
# First GREEN: fxscenarios_mode_intro_roadrage/20260925_110455 (exe 1c59049819d0, 5/5: countdown 6.000000, IN_PROGRESS 9525 lines later).
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxScenariosModeIntroRoadRageLive.ps1
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxscenarios_mode_intro_roadrage'
$case.Area = 'mode'
$case.Bug = 'An offline Road Rage must keep the console intro length (slot 8 -> 6.0 for mode type 3) and still reach E_GMS_IN_PROGRESS after the base GetIntroDurationSeconds stub was replaced by the console body.'
$case.Run.Boost = ''
$case.Run.MaxSeconds = 75
$case.DiagEnv = 'BRN_INTRO_TIMER_DIAG=1,BRN_PROP_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'Script'; Name = 'the road-rage intro is the console length for its mode (type 3 -> 6.0, flt_820211C8)'; Script = {
        param($ctx)
        $inv = [cultureinfo]::InvariantCulture
        $rows = @($ctx.LogLines | Where-Object { $_ -match '\[mode-intro\] IntroState::OnEnter mode type (\d+) ' })
        $rage = @($rows | Where-Object { $_ -match 'mode type 3 ' })
        $bad = @($rage | Where-Object { -not ($_ -match "countdown ([-+0-9.eE]+) s timed (\d)" -and [double]::Parse($Matches[1], $inv) -eq 6.0) })
        @{ Pass = ($rage.Count -gt 0 -and $bad.Count -eq 0); Detail = "$($rows.Count) [mode-intro] line(s), $($rage.Count) for mode type 3, off the console 6.0: $($bad.Count). $(($rows | Select-Object -First 3 | ForEach-Object { $_.Trim() }) -join ' | ')" }
    } }
    @{ Kind = 'Script'; Name = 'the road rage still reaches E_GMS_IN_PROGRESS after its intro'; Script = {
        param($ctx)
        $intro = -1; $prog = -1
        for ($i = 0; $i -lt $ctx.LogLines.Count; $i++) {
            $l = $ctx.LogLines[$i]
            if ($intro -lt 0 -and $l -match '\[mode-intro\] IntroState::OnEnter mode type 3 ') { $intro = $i }
            if ($intro -ge 0 -and $l -match '\[stunt\] mode state -> E_GMS_IN_PROGRESS') { $prog = $i; break }
        }
        @{ Pass = ($intro -ge 0 -and $prog -gt $intro); Detail = "road-rage intro at log line $intro; IN_PROGRESS at log line $prog" }
    } }
)
$case
