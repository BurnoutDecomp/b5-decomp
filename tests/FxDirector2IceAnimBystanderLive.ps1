# FX-DIRECTOR2 (crash parity 2026-09-25) CC-13 live case: a won race's post-event World_Win_* take aims the camera at
# the winner through BehaviourIceAnim's BYSTANDER look-space Looker.
#
# BehaviourIceAnim::Update @0x82247108 runs Looker::Update (0x82247888) on the bystander vehicle whenever the take looks
# in eICE_BYSTANDER_SPACE (11). The PC had left it as a FLAG. 36 retail takes look in space 11 (chaincheck2/icetakes.txt).
# Their reach, as measured:
#   - the 21 World_Win_* are the won-race post-event shots. ArbStatePostEvent::Prepare picks the finish line's win
#     shot (PickAppropriateShot on GameState::mbWonLastEvent) and binds the post-event cam's bystander ref to the player
#     car (SetBystanderRefForPostEvent). THIS CASE.
#   - World_Signature_4 / _11 / _71 look in space 11 in a later interval (the signature-jump outros).
#   - Takedown_ICE_1 is NOT on the shutdown takedown's path. The takedown shot group holds ONE shot, Takedown_ICE_Shut
#     (guid 554362, look [8, 12, 8]): fxdirector2_iceanim_bystander/20260925_220559, under the console's own debug
#     toggle "Always do shutdown TD camera" (BRN_ALWAYS_SHUTDOWN_CAM=1).
# The race is FxTrafficLightsRaceArmsLive.ps1's (junction 480886, -Teleport "3003.9,6.6,-1675.6,0" -StartEvent), driven
# by the game's own AI on the pad (--ai-pad race). It must be WON FOR REAL. The harness debug finish (-DebugFinishPos 1)
# cannot stand in for a win:
#   - ModeManager::ShowModeResults @0x823436D0 reads GetPlayersFinishPosition (0x823439E8), which consumes the debug
#     override, BEFORE HasPlayerWon (0x823439F4);
#   - so the record carries position 1 with hasPlayerWon 0 (fxdirector2_iceanim_bystander/20260925_214731).
# Measured so far, on exes with CC-13: the AI-pad race finished 2nd (20260925_215630) and 5th (20260925_220922). Both
# runs played the finish line's LOST shot with no new assertion. No race was won, so the BYSTANDER arm is not yet
# witnessed live; run_fxdirector2_iceanim_bystander_looker.py is its witness.
# Witness (NOT X360, BRN_CRASHCAM_DIAG): one line per take frame 0..3, then every 30th:
#   [iceanim] bystander look take <guid> '<name>' frame N: aim <before> -> <after> deg, fov <before> -> <after>, ...
# "aim" is the angle between the camera's forward and the winner. "Before" is after the arm's re-focus; "after" is
# after the looker.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxDirector2IceAnimBystanderLive.ps1 --ai-pad race --no-frames --run-name fxdirector2_iceanim_bystander
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxdirector2_iceanim_bystander'
$case.Area = 'director'
$case.Bug = 'A take that looks in the BYSTANDER space (the 21 World_Win_* race-win shots, World_Signature_4 / _11 / _71) must aim and zoom the camera onto the bystander vehicle through BehaviourIceAnim''s Looker (CC-13).'
$case.Run.Teleport = '3003.9,6.6,-1675.6,0'
$case.Run.MaxSeconds = 150
$case.DiagEnv += ',BRN_MODEMGR_DIAG=1,BRN_FINISHLINE_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'the race was won (ShowModeResults: position 1, hasPlayerWon 1)'; Pattern = '\[evt-finish\] ShowModeResults mode 0 finishPosition 1 .* hasPlayerWon 1'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'the post-event camera looked up the finish line''s shots'; Pattern = '\[finish-line\] post-event shots: mode'; Expect = $true }
    @{ Kind = 'LogCount'; Name = 'CC-13: the looker ran on a World_Win_* take'; Pattern = '^\[iceanim\] bystander look take -?\d+ ''World_Win_'; Min = 1 }
    @{ Kind = 'Script'; Name = 'CC-13: the looker frames (aim before -> after, FOV before -> after)'; Script = {
        param($ctx)
        $lines = @($ctx.LogLines | Where-Object { $_ -match '^\[iceanim\] bystander look take ' })
        $first = $lines | Where-Object { $_ -match ' frame 0: aim ([-\d.]+) -> ([-\d.]+) deg' } | Select-Object -First 1
        $pass = $false
        if ($first -and ($first -match ' frame 0: aim ([-\d.]+) -> ([-\d.]+) deg')) {
            $before = [double]::Parse($Matches[1], [cultureinfo]::InvariantCulture)
            $after = [double]::Parse($Matches[2], [cultureinfo]::InvariantCulture)
            $pass = ($after -ge 0) -and ($after -lt $before)
        }
        @{ Pass = $pass
           Detail = ("{0} line(s); the first take frame must turn the camera toward the winner (after < before): {1}" -f $lines.Count, (($lines | Select-Object -First 8 | ForEach-Object { $_ -replace '^\[iceanim\] bystander look ', '' }) -join ' | ')) }
    } }
    @{ Kind = 'Script'; Name = 'the finish (report)'; Script = {
        param($ctx)
        $results = @($ctx.LogLines | Where-Object { $_ -match '^\[evt-finish\] ShowModeResults ' } | ForEach-Object { $_ -replace ' \(1 == WIN.*?\)', '' })
        @{ Pass = $true; Detail = ($results -join ' | ') }
    } }
)
$case
