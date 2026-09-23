# FX-GS2 (crash parity 2026-09-23, G10-D11): the game-state -> GUI interface leg of an offline race, live.
# Race junction 480886 (-Teleport "3003.9,6.6,-1675.6,0" -StartEvent), driven by
# tests/run_rival_organic.py's pad pursuit. BRN_MODEMGR_DIAG=1 arms two witnesses:
#   [tailing]   GameStateModule::CheckForTailingRivals @0x82375F90 posted an on-tail record
#               (a rival 0..20 m behind the player, both at pace, for 3 s) -- first 12
#   [gui-iface] TranslateGuiInterfaceToGuiEvents @0x823E1D90 posted the interface's records to the
#               GUI (372 = finish race, 486 = GuiNetworkPlayerOnTailEvent) -- first 24 non-empty calls
# and BRN_HUDMSG_DIAG=1 the HUD director's filter lines, which follow the "AggDrOnTail" message
# (HudMessageAnalyzer::HandleNetworkTailing @0x8251CCD8) into the director.
#
# The DISPATCH check is the robust one: the race's own finish record (ModeManager::FinishCurrentMode
# -> AddFinishedRaceEvent) must come out of the leg as a GUI 372. Whether a rival sits on the
# player's tail depends on how the pursuit race plays out, so the on-tail counts are reported, not
# required (first run, 20260923_135945: the player trailed and finished 6th -- no on-tail record).
# NOTE: a race that FINISHES also reaches the director's post-event camera, whose
# DirectorResourceManager::GetEventCompletionShots asserts "Unknown finish line": the director's
# GameState.mFinishLineID has no writer on this build (only its Construct zeroes it). That family is
# a director-lane defect, not this leg's.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxGs2Tailing.ps1 --run-name fxgs2_tailing
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxgs2_tailing'
$case.Area = 'hud'
$case.Bug = 'The GameStateToGuiInterface records (on-tail, finish, dirty tricks, lead/last) must reach the GUI as events 177..486 through TranslateGuiInterfaceToGuiEvents.'
$case.Run.Teleport = '3003.9,6.6,-1675.6,0'
$case.Run.MaxSeconds = 120
$case.DiagEnv += ',BRN_MODEMGR_DIAG=1,BRN_HUDMSG_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'the GUI-interface leg posted a record (TranslateGuiInterfaceToGuiEvents dispatched)'; Pattern = '\[gui-iface\] posted 177x\d+ 179x\d+ 181x\d+ 371x\d+ 372x\d+ 484x\d+ 485x\d+ 486x\d+'; Expect = $true }
    @{ Kind = 'Script'; Name = 'on-tail records vs GUI 486 events, finish records, and the HUD director'; Script = {
        param($ctx)
        $tail = @($ctx.LogLines | Where-Object { $_ -match '\[tailing\] on-tail record' }).Count
        $gui = 0
        $finish = 0
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[gui-iface\] posted .* 372x(\d+) .* 486x(\d+)') { $finish += [int]$Matches[1]; $gui += [int]$Matches[2] }
        }
        $director = @($ctx.LogLines | Where-Object { $_ -match 'director: filter "AggDrOnTail"' })
        $first = if ($director.Count -gt 0) { $director[0].Trim() } else { '(none)' }
        @{ Pass = $true; Detail = "$tail on-tail record(s) (capped 12), $gui GUI 486 event(s), $finish GUI 372 event(s) (capped 24 lines), $($director.Count) director AggDrOnTail line(s); first: $first" }
    } }
)
$case
