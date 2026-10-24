# Run through tools/tests/run_case.ps1. BP_TEST_COLLECTIBLE selects jumps (default),
# smashes or billboards. Only the runner's pinned profile is modified.
$family = if ($env:BP_TEST_COLLECTIBLE) { $env:BP_TEST_COLLECTIBLE } else { 'jumps' }
$types = @{ jumps = 0; smashes = 1; billboards = 2 }
$trophies = @{ jumps = 2; smashes = 3; billboards = 1 }
if (-not $types.ContainsKey($family)) { throw "Unknown collectible family: $family" }
$expectedType = $types[$family]
$expectedTrophy = $trophies[$family]

@{
  Name = "collectible_reward_resume_$family"
  Area = 'progression'
  Bug = 'b5-decomp #50: control and the gameplay camera must return after the collectible car award'
  Frames = $false
  ProfileFixture = 'rival_hunt_profile.sav'
  Run = @{
    Drive = $true
    DriveDelay = 15.0
    TitleStart = $true
    MotionProbe = $true
    MaxSeconds = 100
    SkipIntro = $true
    AcceptGap = 1.0
    Teleport = '3040.7,-5.8,-1937.9,180'
  }
  DiagEnv = "BRN_PROGRESSION_COMPLETION_SEEDTROPHY=freeroam:$family,BRN_PROGRESSION_COMPLETION=1,BRN_COLLECT_DIAG=1,BRN_RACEFLOW_DIAG=1,BRN_TROPHY_DIAG=1,BRN_CRASHCAM_DIAG=1"
  Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'LogMatch'; Name = 'original complete-all action ran'; Pattern = "\[collect\] complete-all type=$expectedType trophy=$expectedTrophy collected=" }
    @{ Kind = 'LogMatch'; Name = 'the real collectible award reached the GUI'; Pattern = "\[race-flow\] action 204 -> gui 375 \(unlock type $expectedTrophy\)" }
    @{ Kind = 'LogCount'; Name = 'one award presentation'; Pattern = '\[trophy\] screen enter'; Min = 1; Max = 1 }
    @{ Kind = 'LogMatch'; Name = 'award presentation completed'; Pattern = '\[trophy\] screen advance' }
    @{ Kind = 'Script'; Name = 'camera and player control returned after the award'; Script = {
        param($ctx)
        $left = $false; $cameraReturned = $false; $playerReturned = $false
        $points = @(); $gasSamples = 0
        foreach ($line in $ctx.LogLines) {
          if ($line -match '\[trophy\] screen leave') { $left = $true; continue }
          if (-not $left) { continue }
          if ($line -match '\[pause-cam\] arbitrator CRASH_NAV_ICE_CAMERAS -> NORMAL') { $cameraReturned = $true }
          if ($line -match '\[ai-evt\] PLAYER_TAKEN_OVER inControl 1') { $playerReturned = $true }
          if ($line -match '^\[motion\].* pos (\S+) (\S+) (\S+) .* gas (\S+)') {
            $points += ,@([double]::Parse($Matches[1], [Globalization.CultureInfo]::InvariantCulture),
                         [double]::Parse($Matches[3], [Globalization.CultureInfo]::InvariantCulture))
            if ([double]::Parse($Matches[4], [Globalization.CultureInfo]::InvariantCulture) -gt 0.1) { ++$gasSamples }
          }
        }
        $distance = 0.0
        for ($i = 1; $i -lt $points.Count; ++$i) {
          $dx = $points[$i][0] - $points[$i-1][0]; $dz = $points[$i][1] - $points[$i-1][1]
          $step = [Math]::Sqrt($dx*$dx + $dz*$dz)
          if ($step -lt 20.0) { $distance += $step }
        }
        @{ Pass = ($left -and $cameraReturned -and $playerReturned -and $gasSamples -ge 5 -and $distance -ge 10.0)
           Detail = "left=$left camera=$cameraReturned player=$playerReturned gasSamples=$gasSamples distance=$distance" }
      } }
  )
}
