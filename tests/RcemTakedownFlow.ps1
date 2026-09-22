# FX-RCEM (crash-parity 2026-09-22): the takedown flow's world-side consumers, live.
# Built on tools/tests/cases/takedown_forced.ps1: a Road Rage event with the console's own
# "Force takedown" debug action (player -> car 1) after 12 s, so the player-aggressor takedown
# camera runs deterministically. BRN_TD_DIAG=1 arms the [td-action] witness in
# RaceCarEntityModule::HandleGameActions.
#   * StartTakedownCamera posts action 111 for every offline player takedown, so the
#     invulnerability arm must be DISPATCHED (ARTIST 0x8230D230: the player slot's
#     mfInvulnerablityTime = camera time + KF_POST_TAKEDOWN_INVULNERABLE_TIME 2.5).
#   * Action 3 (the takedown-camera reset, 0x8230C720) fires only when the player car is slow,
#     airborne or spinning inside the camera, so it is reported, not required.
#   * Actions 120/121 are free-burn only; tests/run_rcem_takedown_actions.py covers them.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/RcemTakedownFlow.ps1
$workflow = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$case = & (Join-Path $workflow 'tools/tests/cases/takedown_forced.ps1')
$case.Name = 'rcem_takedown_flow'
$case.Bug = 'A player takedown must make the player invulnerable (action 111) and may reset it on track (action 3).'
$case.DiagEnv += ',BRN_TD_DIAG=1'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'action 111 reached the race-car module'; Pattern = '\[td-action\] 111 PLAYER_INVULNERABLE'; Expect = $true }
    @{ Kind = 'Script'; Name = 'invulnerability time is the camera time plus 2.5 s'; Script = {
        param($ctx)
        $times = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[td-action\] 111 PLAYER_INVULNERABLE player slot \d+ time ([-+0-9.eE]+)') {
                [double]::Parse($Matches[1], [cultureinfo]::InvariantCulture)
            }
        })
        $bad = @($times | Where-Object { $_ -le 2.5 })
        @{ Pass = ($times.Count -gt 0 -and $bad.Count -eq 0); Detail = "times: $($times -join ', ') (each must exceed KF_POST_TAKEDOWN_INVULNERABLE_TIME 2.5)" }
    } }
    @{ Kind = 'Script'; Name = 'takedown-camera resets (action 3) reported'; Script = {
        param($ctx)
        $resets = @($ctx.LogLines | Where-Object { $_ -match '\[td-action\] 3 RESET_PLAYER_CAR_ON_TRACK' })
        @{ Pass = $true; Detail = "$($resets.Count) action-3 arm(s): $(($resets | Select-Object -First 2) -join ' | ')" }
    } }
)
$case
