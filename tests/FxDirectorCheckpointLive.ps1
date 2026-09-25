# FX-DIRECTOR (crash parity 2026-09-24) -- the director converts a checkpoint's GLOBAL race-car index, live.
#
# BridgeWorldToDirector step 8 (0x823E3FD8..0x823E3FEC) copies the world's RCEntityGlobalRaceCarOutputInterface
# into the director input at +0x10, and MainDirector::ProcessInputQueue case 113 (@0x822385D4) converts each
# E_ACTION_RACE_CAR_REACHED_CHECKPOINT record's global index through it (GetActiveRaceCarIndex) and compares
# the result with the player's active index: equal -> GameState +0x1B4 mbPlayerHitCheckpointThisFrame.
# Its reader, ArbStateRoaming::ProcessPossibleFX's "Checkpoint" hook, runs in the online event types
# 10 / 11 / 13 / 15 / 16 only, so the visible hook cannot be shown offline (on the console either). What an
# offline race shows is the conversion: every car's checkpoint posts 113, the rivals' convert to their own
# active slots (never -1: the table was published) and leave the flag down, and the player's converts to
# the player's slot and raises it.
#
# The race is FxAinan2CheckpointLive.ps1's / FxGs2Tailing.ps1's (junction 480886, one checkpoint -- the
# finish), driven by tests/run_rival_organic.py's pad pursuit. (-AIDrive was tried first: the AI drove the
# car off the junction before the start injection fired and started a Stunt Run elsewhere --
# fxdirector_checkpoint/20260924_194006.) The player finishing is up to the pursuit (fxgs2_tailing/
# 20260923_135945 finished 6th, all six cars posting 113), hence the longer budget (180 s was not enough in
# fxdirector_checkpoint/20260924_195817: the five rivals' 113s converted g1..g5 -> a1..a5, the player had not
# reached the finish).
# Witness (BRN_DIRECTOR_ACTION_DIAG, NOT X360):
#   [director-action] 113 RACE_CAR_REACHED_CHECKPOINT global G -> active A (player P) -> mbPlayerHitCheckpointThisFrame F
# FX-AIPAD (2026-09-25): with the game's own AI on the pad (--ai-pad race, BRN_AI_PAD_PLAYER=race) the player
# finished 2nd at 49 s in mode, 1 s behind the winner (fxaipad_race/20260925_102436); the pad pursuit's banked runs
# finished 6th at 156 s (fxdirector_checkpoint/20260924_202224) or not at all (195817, 20260925_072059, 074632).
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxDirectorCheckpointLive.ps1 --ai-pad race --run-name fxdirector_checkpoint
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxDirectorCheckpointLive.ps1 --run-name fxdirector_checkpoint   (the pad pursuit)
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxdirector_checkpoint'
$case.Area = 'director'
$case.Bug = 'ProcessInputQueue 113 must convert a checkpoint''s global race-car index through the input''s global table (BridgeWorldToDirector step 8) and raise +0x1B4 for the player''s checkpoint only.'
$case.Run.Teleport = '3003.9,6.6,-1675.6,0'
$case.Run.MaxSeconds = 300
$case.DiagEnv += ',BRN_DIRECTOR_ACTION_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'action 113 reached the director arm'
       Pattern = '\[director-action\] 113 RACE_CAR_REACHED_CHECKPOINT global \d+ -> active'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'every checkpoint car converted to a live active slot (the table was published: no -1)'
       Pattern = '\[director-action\] 113 RACE_CAR_REACHED_CHECKPOINT global \d+ -> active -1 '; Max = 0 }
    @{ Kind = 'Script'; Name = 'the player''s checkpoint raised +0x1B4, and a rival''s alone did not'; Script = {
        param($ctx)
        $player = 0; $playerRaised = 0; $rival = 0; $rivalClear = 0; $samples = @()
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[director-action\] 113 RACE_CAR_REACHED_CHECKPOINT global (\d+) -> active (-?\d+) \(player (-?\d+)\) -> mbPlayerHitCheckpointThisFrame (\d)') {
                if ($samples.Count -lt 8) { $samples += "g$($Matches[1])->a$($Matches[2])/p$($Matches[3])=$($Matches[4])" }
                if ($Matches[2] -eq $Matches[3]) { $player++; if ($Matches[4] -eq '1') { $playerRaised++ } }
                else { $rival++; if ($Matches[4] -eq '0') { $rivalClear++ } }
            }
        }
        @{ Pass = ($player -gt 0 -and $playerRaised -eq $player -and $rivalClear -gt 0)
           Detail = ("player checkpoints {0} (raised {1}), rival checkpoints {2} (flag clear {3}); {4}" -f $player, $playerRaised, $rival, $rivalClear, ($samples -join ' ')) }
    } }
    @{ Kind = 'LogCount'; Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
)
$case
