# FX-FOLLOWUPS item 4 (crash parity 2026-09-25): the persistent-damage RE-COLOUR, live. One Road Rage run (the
# RivalOrganic start event) in which the game's own AI drives the player car at the nearest rival (--ai-pad pursuit).
#
# The console re-colours a taken-down rival in a persistent-damage mode (KU_FLAG_AI_PERSISTENT_DAMAGE) on two legs
# of RaceCarEntityModule::ProcessRaceCarCrashCompleteEvents @0x822F3FE0:
#   * 0x822F4174 -- no damage to add: the rival is clean and three race cars already carry damage
#     (GetPersistentDamageCarCount() >= 3), so it only takes a new colour;
#   * 0x822F4298 -- IncreasePersistentDamage wrapped: 0.3 more damage reached 1.0 (the FOURTH credited takedown
#     of one rival), the damage restarts at 0.0 and the rival takes a new colour.
# Both: miColourIndex = GetRandomCarColour(palette, -1), or 6 under KU_FLAG_SET_OPPONENTS_TO_COPS; the next
# UpdateActiveRaceCarColours (every PostPhysics update) repaints the rival from it.
# fxrcem4_live 20260924_195203 (default driver, 200 s) credited two AI takedowns, both first ones: neither leg ran.
# fxfollowups_recolour 20260925_173429 (pursuit, 200 s) credited 12, and took the CLEAN leg organically 5 times
# (cars 2, 1, 3 damaged first); the wrap leg needs a fourth takedown of one rival (car 3 reached 0.9).
#
# BRN_PERSIST_DAMAGE_PRELOAD=wrap (default-off harness; RecolourScenarioHarness, BrnRaceCarEntityModule_CrashExit.cpp):
# at the FIRST credited AI takedown it runs the console's own IncreasePersistentDamage three times on the victim
# and two other attached rivals (0.9 each, 3 damaged cars -- a state the console reaches organically), so the
# victim's crash complete takes the WRAP leg; later clean victims meet three damaged cars (the clean leg).
# BRN_CRASH_EXIT_DIAG prints each [persist-damage] decision and the repaint; BRN_RACECAR_PAINT_DIAG the
# rivals' SetupCarColour picks.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxFollowupsRecolourLive.ps1 --run-name fxfollowups_recolour --ai-pad pursuit --no-frames
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxfollowups_recolour'
$case.Bug = 'A taken-down rival in a persistent-damage mode must be re-coloured on both console legs (0x822F4174 / 0x822F4298) and repainted.'
$case.DiagEnv += ',BRN_PERSIST_DAMAGE_PRELOAD=wrap,BRN_CRASH_EXIT_DIAG=1,BRN_RACECAR_PAINT_DIAG=1'
$case.Run.MaxSeconds = 200
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogValue'; Name = 'the harness preloaded the first victim and two rivals (0.9 each, 3 damaged cars)'
       Pattern = '\[persist-preload\] HARNESS wrap victim \d+ car \d+ damage [-0-9.e]+ -> 0\.9\d* damaged cars (?<n>\d+)'; Group = 'n'; Agg = 'max'; Min = 3 }
    @{ Kind = 'LogMatch'; Name = 'WRAP leg 0x822F4298: 0.9 -> 0.0, recolour 1'; Pattern = '\[persist-damage\] car \d+ damage 0\.9\d* -> 0\.0+ recolour 1 colour \d+ -> \d+'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'the re-coloured rival was repainted (a colour change from a colour)'; Pattern = '\[persist-damage\] repaint car \d+ colour \d+ -> \d+ REPAINTED'; Expect = $true }
    @{ Kind = 'LogCount'; Name = 'no re-colour went unrepainted'; Pattern = '\[persist-damage\] repaint car \d+ colour \d+ -> \d+ NOT repainted'; Max = 0 }
    @{ Kind = 'Script'; Name = 'the two re-colour legs (report)'; Script = {
        param($ctx)
        $re = @($ctx.LogLines | Where-Object { $_ -match '\[persist-damage\] car \d+ damage .* recolour 1 ' })
        $wrap  = @($re | Where-Object { $_ -match 'damage 0\.9\d* -> 0\.0+ recolour 1' })
        $clean = @($re | Where-Object { $_ -match 'damage 0\.0+ -> 0\.0+ recolour 1' })
        @{ Pass = $true; Detail = "$($re.Count) re-colour(s): wrap leg 0x822F4298 x$($wrap.Count), clean leg 0x822F4174 x$($clean.Count)" +
           $(if ($re.Count) { '; ' + (($re | Select-Object -First 4 | ForEach-Object { $_.Trim() }) -join ' | ') } else { '' }) }
    } }
    @{ Kind = 'Script'; Name = 'repaints (report)'; Script = {
        param($ctx)
        $l = @($ctx.LogLines | Where-Object { $_ -match '\[persist-damage\] repaint car' })
        @{ Pass = $true; Detail = "$($l.Count) line(s)" + $(if ($l.Count) { '; ' + (($l | Select-Object -First 4 | ForEach-Object { $_.Trim() }) -join ' | ') } else { '' }) }
    } }
    @{ Kind = 'Script'; Name = 'every persistent-damage decision (report)'; Script = {
        param($ctx)
        $l = @($ctx.LogLines | Where-Object { $_ -match '\[persist-damage\] car \d+ damage|\[persist-preload\]' })
        @{ Pass = $true; Detail = "$($l.Count) line(s)" + $(if ($l.Count) { '; ' + (($l | Select-Object -First 8 | ForEach-Object { $_.Trim() }) -join ' | ') } else { '' }) }
    } }
)
$case
