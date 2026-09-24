# FX-FLOW (crash parity 2026-09-24, G11-D1 remainder, consumer half): the race-mode HUD message arms of
# BrnGameModule::TranslateGameActionsToGuiEvents, live. Same drive as FX-GS's FxGsRaceHudCrashes.ps1:
# race junction 480886 (-Teleport "3003.9,6.6,-1675.6,0" -StartEvent), tests/run_rival_organic.py's
# pad pursuit so rivals collide. HUDMessageLogic::DetectCrashes already posts action 250 in an offline
# race; the translator's new case 250 (@0x823EC0CC) must turn it into GUI 482
# (AddGuiEvent<GuiNetworkPlayerCrashingEvent>). BRN_MODEMGR_DIAG=1 arms both witnesses:
#   `[hud-xcrash] action 250 posted: ...`    (the producer, DetectCrashes)
#   `[race-hud] action 250 -> gui 482 ...`   (the new translator arm)
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxFlowRaceHudBridge.ps1 --run-name fxflow_race_hud_bridge
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxflow_race_hud_bridge'
$case.Area = 'hud'
$case.Bug = 'The race-mode HUD actions (242/245/246/247/248/250) must reach the GUI as their console events.'
$case.Run.Teleport = '3003.9,6.6,-1675.6,0'
$case.Run.MaxSeconds = 240   # long enough for a rival to crash into traffic (DetectCrashes -> 250)
$case.DiagEnv += ',BRN_MODEMGR_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'the producer posted action 250 in the offline race'; Pattern = '\[hud-xcrash\] action 250 posted: crashed slot \d+ .*mode 0,'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'the translator turned action 250 into GUI 482 (case 250 @0x823EC0CC)'; Pattern = '\[race-hud\] action 250 -> gui 482 \(car slot \d+'; Expect = $true }
    @{ Kind = 'Script'; Name = 'race HUD bridge lines reported'; Script = {
        param($ctx)
        $lines = @($ctx.LogLines | Where-Object { $_ -match '\[race-hud\] action (\d+) -> gui (\d+)' })
        $pairs = @($lines | ForEach-Object { if ($_ -match '\[race-hud\] action (\d+) -> gui (\d+)') { "$($Matches[1])->$($Matches[2])" } })
        @{ Pass = $true; Detail = "$($lines.Count) line(s) (witness capped at 24): $(($pairs | Select-Object -Unique) -join ', ')" }
    } }
)
$case
