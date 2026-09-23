# FX-GS (crash parity 2026-09-23, G11-D1): the offline race's "X crashes" HUD records, live.
# Race junction 480886 (-Teleport "3003.9,6.6,-1675.6,0" -StartEvent), driven by
# tests/run_rival_organic.py's pad pursuit so rivals collide. BRN_MODEMGR_DIAG=1 arms the
# `[hud-xcrash]` witness in HUDMessageLogic::DetectCrashes (@0x82394418), which prints the latched
# mode and the HUD queue length for every action-250 record it posts (first 20).
#   * The race arm (PostWorldUpdate case 0 -> GenerateRaceModeMessages @0x82399C78 -> DetectCrashes)
#     must be DISPATCHED: at least one `[hud-xcrash] ... mode 0` line.
#   * It must not overflow the HUD queue (VariableEventQueue<256,16>): no new assert family.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxGsRaceHudCrashes.ps1 --run-name fxgs_g11_race
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxgs_race_hud_crashes'
$case.Area = 'hud'
$case.Bug = 'An offline race must post one HUD "X crashes" record (action 250) per non-player crash event.'
$case.Run.Teleport = '3003.9,6.6,-1675.6,0'
$case.Run.MaxSeconds = 120
$case.DiagEnv += ',BRN_MODEMGR_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'the offline race arm posted an action-250 record (DetectCrashes dispatched)'; Pattern = '\[hud-xcrash\] action 250 posted: crashed slot \d+ .*mode 0,'; Expect = $true }
    @{ Kind = 'Script'; Name = 'action-250 records reported'; Script = {
        param($ctx)
        $slots = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[hud-xcrash\] action 250 posted: crashed slot (\d+)') { [int]$Matches[1] }
        })
        @{ Pass = $true; Detail = "$($slots.Count) record(s) (witness capped at 20), slots: $(($slots | Select-Object -Unique) -join ', ')" }
    } }
)
$case
