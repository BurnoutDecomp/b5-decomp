# FX-RCEM4 (crash parity 2026-09-24): one live Road Rage run (the RivalOrganic start event: junkyard
# boot -> car select -> exit -> DRIVING -> event start with rivals) over the world-side arms this lane
# lands, with the BRN_RCEM_ACTION_DIAG dispatch witnesses armed:
#   * G68-D11 case 4 (HandleSetPlayerOpponentsAction @0x822E96D8) -- GameStateModule::OnPlayerCarChange
#     posts the new car's opponent set at the junkyard exit; `[rcem-action] 4 player opponents N`.
#   * G68-D11 case 74 (the junkyard exit's audio wait @0x8230C454) -- CarSelectManager::ExitJunkyard
#     posts byte 1; `[rcem-action] 74 audio wait armed: waiting 1, streamer wait 1, module latch 1,
#     waiting for streaming 1`, then UpdateStreaming's +0x186D1 leg must RELEASE it once the player's
#     streaming sound is attached: `[rcem-action] 74 audio wait released` (a run with the first line
#     and not the second is a streaming-complete edge held forever).
#   * BRN_ENGINE_DIAG=1 adds the [car-audio] slot transitions, so the release can be matched to the
#     player's slot reaching ATTACHED/LOADEDANDATTACHED.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxRcem4Live.ps1 --run-name fxrcem4_live
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxrcem4_live'
$case.Bug = 'The FX-RCEM4 HandleGameActions arms (4, 74) and the junkyard-exit audio wait must be dispatched and released with no new asserts.'
$case.DiagEnv += ',BRN_RCEM_ACTION_DIAG=1,BRN_ENGINE_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'G68-D11 case 4 (player opponents) dispatched'; Pattern = '\[rcem-action\] 4 player opponents \d+'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'G68-D11 case 74 armed the junkyard-exit audio wait'; Pattern = '\[rcem-action\] 74 audio wait armed: waiting 1, streamer wait 1, module latch 1, waiting for streaming 1'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'G68-D11 74: UpdateStreaming released the audio wait'; Pattern = '\[rcem-action\] 74 audio wait released'; Expect = $true }
    @{ Kind = 'Script'; Name = 'G68-D11 4: opponent set (report)'; Script = {
        param($ctx)
        $l = @($ctx.LogLines | Where-Object { $_ -match '\[rcem-action\] 4 player opponents' })
        @{ Pass = $true; Detail = $(if ($l.Count) { "$($l.Count) line(s); first: $($l[0].Trim())" } else { 'none' }) }
    } }
)
$case
