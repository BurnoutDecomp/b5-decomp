# FX-RCEM2 (crash parity 2026-09-23): one live Road Rage run (the RivalOrganic start event,
# mode 3, KU_FLAG_AI_DRIVE_BY_START set) over the world-side arms this lane landed:
#   * G68-D5 HandleGameActions case 29 arms mfIntroTimer and UpdateRaceCars_PreScene @0x822F5578
#     counts it down -> the `[intro-timer] armed` / `[intro-timer] expired` witnesses (the second is
#     printed from inside UpdateRaceCars_PreScene, so it also proves the per-slot Update_PreScene
#     pass -- G61-D2's mbCrashedIntoWater clear -- was DISPATCHED).
#   * G67-D4/D5 ResetActiveRaceCar's live-car arm (the `[teleport] ... RE-RESET` line is printed on
#     that arm right before the new SetBit / glass-clear legs).
#   * G68-D6 action 205 on a player crash -> the `[reset-mirror]` line of the next player reset
#     (BRN_CRASH_RESPONSE_DIAG) carries mirrorType 0 (reported, not required: the player may not crash).
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/Rcem2Live.ps1 --run-name rcem2_live
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'rcem2_live'
$case.Bug = 'The pre-scene pass, the drive-by intro countdown and the reset/glass legs must run with no new asserts.'
$case.DiagEnv += ',BRN_CRASH_RESPONSE_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'G68-D5 case 29 armed the intro timer'; Pattern = '\[intro-timer\] armed'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'G68-D5 UpdateRaceCars_PreScene released the rivals'; Pattern = '\[intro-timer\] expired -> SetAllCarsOnStartLine\(ROLLING_START'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'G67-D4/D5 the live-car reset arm ran'; Pattern = '\[teleport\] ResetActiveRaceCar RE-RESET car'; Expect = $true }
    @{ Kind = 'Script'; Name = 'G68-D6 player reset mirrors (report)'; Script = {
        param($ctx)
        $rows = @($ctx.LogLines | Where-Object { $_ -match '\[reset-mirror\] car \d+ isPlayer 1' } |
                  ForEach-Object { if ($_ -match 'mirrorType (-?\d+) mirrorAmount ([^ ]+)') { "$($Matches[1])/$($Matches[2])" } } |
                  Select-Object -Unique)
        @{ Pass = $true; Detail = "player mirror (type/amount) values seen: $($rows -join ', ')" }
    } }
)
$case
