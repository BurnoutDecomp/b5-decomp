# L2 CAMCOLLIDE live case (owner's list 2026-09-27 / 28: "The camera tend to be behind walls or below the map for things
# like crashes/takedowns/junkyard") -- the TAKEDOWN shots.
#
# The organic take-down drive (RivalOrganic: an event started at the teleport, the game's own AI on the pad with
# --ai-pad pursuit ramming the rivals), with the BRN_CAMCOLLIDE_DIAG witness: every presented frame's eye is tested
# against the world (inside a 0.1 m sphere, the ground above / below it, the eye <-> car line) and attributed to the
# arbitrator state that owned the frame ([crashcam] container current state, BRN_CRASHCAM_DIAG). One strip of frames
# from the first slow motion (BRN_FRAME_DUMP_ARM=slomo) is dumped for the before / after pair.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/L2CamCollideTakedownLive.ps1 --ai-pad pursuit --slot 2 --run-name l2_td_default
# The director's complete scene-query chain runs by default.
# Seed the slot's Memcard_<n>\Profile.sav from the SAME returning save before each run of a pair, and check the car:
# since f935feb8 the save's own car reaches the game (the crash-sweep seed 2524d4e6 now drives VEH_PASBSC01).
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'l2_camcollide_takedown'
$case.Area = 'camera'
$case.Bug = 'Take-down shots must not film from inside or behind the world (the owner''s "camera behind walls").'
$case.Frames = $true
$case.Run.MaxSeconds = 240
$case.Run.FrameEvery = 2
$case.DiagEnv += ',BRN_CAMCOLLIDE_DIAG=1,BRN_VLQ_DIAG=1,BRN_FRAME_DUMP_ARM=slomo,BRN_FRAME_DUMP_MAX=160'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'no volume line-kernel traps'; Pattern = '\[vlq\] TRAP'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'take-down camera states entered'; Pattern = '\[crashcam\] container current state -> 3\b'; Min = 1 }
    @{ Kind = 'LogMatch'; Name = 'the witness ran'; Pattern = '^\[camcol\] f='; Expect = $true }
    @{ Kind = 'Script'; Name = 'the take-down shots'' eyes (witness, per arbitrator state)'; Script = {
        param($ctx)
        $state = 'Boot'; $rows = @{}
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[crashcam\] container current state -> -?\d+ \((\w+)\)') { $state = $Matches[1]; continue }
            if ($line -match '^\[camcol\] f=\d+ .* in=(-?\d) .* UNDER=(\d) OCC=(\d)') {
                if (-not $rows.ContainsKey($state)) { $rows[$state] = @(0, 0, 0, 0) }
                $r = $rows[$state]; $r[0]++
                if ($Matches[1] -eq '1') { $r[1]++ }
                if ($Matches[2] -eq '1') { $r[2]++ }
                if ($Matches[3] -eq '1') { $r[3]++ }
                $rows[$state] = $r
            }
        }
        $td = $rows['ArbStateTakedown']
        $detail = ($rows.Keys | Sort-Object | ForEach-Object { "$($_) $($rows[$_][0]) frames (in $($rows[$_][1]), UNDER $($rows[$_][2]), OCC $($rows[$_][3]))" }) -join '; '
        @{ Pass = ($null -ne $td -and $td[0] -gt 0); Detail = $detail }
    } }
)
$case
