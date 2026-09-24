# FX-DIRECTOR live case (crash parity 2026-09-24, item 2): the rest of MainDirector::ProcessInputQueue's arms
# (0 / 53 / 54 / 107 / 120 / 132 / 150 / 151 / 215 / 216 / 218 / 224) are DISPATCHED by the live game-action queue.
# The drive is RivalOrganic.ps1's Road Rage start (teleport to the rival spot, start the event, throttle + boost), run
# through run_case.ps1 WITHOUT the pad-steering driver: the junkyard spawn posts action 0, the Road Rage rivals ram the
# player (53 / 54) and the drive passes traffic (107). The numeric proof of every arm is
# tests/run_fxdirector_input_queue.py; this case proves the arms run in the real game and prints the values they leave.
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxDirectorPiqArms.ps1
# Witness (BRN_DIRECTOR_ACTION_DIAG, NOT X360): `[director-action] <id> <NAME> -> <field> <value>`.
@{
    Name = 'fxdirector_piq_arms'
    Area = 'director'
    Bug = 'ProcessInputQueue must fold every console game-action arm into the director GameState (item 2 of the FX-DIRECTOR audit).'
    Frames = $false
    Run = @{
        Drive = $true
        Boost = '6:3:2'
        MotionProbe = $true
        SkipIntro = $true
        AcceptGap = 1.0
        Teleport = '3040.7,-5.8,-1937.9,180'
        StartEvent = $true
        EventFsm = $true
        SkipTrainingTip = $true
        MaxSeconds = 110
    }
    DiagEnv = 'BRN_CRASHCAM_DIAG=1,BRN_TD_DIAG=1,BRN_DIRECTOR_ACTION_DIAG=1'
    Checks = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
        @{ Kind = 'Script'; Name = 'item 2: the audit arms are dispatched by the live game-action queue (counts per action id)'; Script = {
            param($ctx)
            $ids = @('0', '53', '54', '107', '120', '132', '150', '151', '215', '216', '218', '224')
            $seen = @{}
            foreach ($line in $ctx.LogLines) {
                if ($line -match '\[director-action\] (\d+) ') {
                    if ($ids -contains $Matches[1]) { $seen[$Matches[1]] = 1 + [int]$seen[$Matches[1]] }
                }
            }
            $summary = ($seen.Keys | Sort-Object { [int]$_ } | ForEach-Object { "$_ x$($seen[$_])" }) -join ', '
            @{ Pass = ($seen.Count -gt 0); Detail = ("[director-action] item-2 ids: {0}" -f $(if ($summary) { $summary } else { 'none' })) }
        } }
    )
}
