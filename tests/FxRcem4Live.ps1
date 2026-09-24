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
#   * CHAIN-RECOLOUR / G61-D6 colour leg: every rival load now runs SetupCarColour @0x822F5170 on the
#     default pair ActiveRaceCar::OnResourcesLoaded read off the car's burnoutcargraphicsasset --
#     `[car-colour] SetupCarColour slot N player 0 default palette P colour C -> palette P colour C'`
#     (BRN_RACECAR_PAINT_DIAG). A Road Rage takedown that wraps a rival's persistent damage past 1.0
#     (or takes down a clean rival while three others carry damage) re-colours it:
#     `[persist-damage] car N ... recolour 1 colour A -> B` -- organic, so reported, not required.
#   The run is lengthened to 200 s so a rival can collect the fourth takedown the wrap needs.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxRcem4Live.ps1 --run-name fxrcem4_live
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxrcem4_live'
$case.Bug = 'The FX-RCEM4 arms (4, 74, the junkyard-exit audio wait) and the car-colour chain (SetupCarColour, the takedown re-colour) must run with no new asserts.'
$case.DiagEnv += ',BRN_RCEM_ACTION_DIAG=1,BRN_ENGINE_DIAG=1,BRN_RACECAR_PAINT_DIAG=1,BRN_CRASH_EXIT_DIAG=1'
$case.Run.MaxSeconds = 200
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'G68-D11 case 4 (player opponents) dispatched'; Pattern = '\[rcem-action\] 4 player opponents \d+'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'G68-D11 case 74 armed the junkyard-exit audio wait'; Pattern = '\[rcem-action\] 74 audio wait armed: waiting 1, streamer wait 1, module latch 1, waiting for streaming 1'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'G68-D11 74: UpdateStreaming released the audio wait'; Pattern = '\[rcem-action\] 74 audio wait released'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'CHAIN-RECOLOUR: SetupCarColour ran for a rival'; Pattern = '\[car-colour\] SetupCarColour slot \d+ player 0 '; Expect = $true }
    @{ Kind = 'Script'; Name = 'CHAIN-RECOLOUR: rival colour picks (report)'; Script = {
        param($ctx)
        $l = @($ctx.LogLines | Where-Object { $_ -match '\[car-colour\] SetupCarColour' })
        @{ Pass = $true; Detail = "$($l.Count) line(s); " + (($l | Select-Object -First 4 | ForEach-Object { $_.Trim() }) -join ' | ') }
    } }
    @{ Kind = 'Script'; Name = 'CHAIN-RECOLOUR: takedown re-colours (report)'; Script = {
        param($ctx)
        $all = @($ctx.LogLines | Where-Object { $_ -match '\[persist-damage\]' })
        $re  = @($all | Where-Object { $_ -match 'recolour 1' })
        @{ Pass = $true; Detail = "$($all.Count) persist-damage line(s), $($re.Count) re-colour(s)" + $(if ($re.Count) { "; first: $($re[0].Trim())" } else { '' }) }
    } }
    @{ Kind = 'Script'; Name = 'G68-D11 4: opponent set (report)'; Script = {
        param($ctx)
        $l = @($ctx.LogLines | Where-Object { $_ -match '\[rcem-action\] 4 player opponents' })
        @{ Pass = $true; Detail = $(if ($l.Count) { "$($l.Count) line(s); first: $($l[0].Trim())" } else { 'none' }) }
    } }
)
$case
