# L1 (owner's list 2026-09-28, item 2 "The camera tend to be behind walls or below the map for things like
# crashes/takedowns/junkyard", piece 7b) -- the crash state's fallback camera stays on the car's side of the wall, live.
#
# The cell is L2's (scratch/OWNERLIST_0927/L2/l2_sweep.ps1 -Heading 225 -Speed 80): the car launched at 80 m/s from
# 128 m out, heading 225, into the wall at (3170.6, -3.7, -2004.6). The crash state films the hard stop (a HardStop
# moment, ultra slo-mo), then -- "crash camera: moment type=-1" -- falls back to the chase cam,
# SharedCameraContainer::mGameplayExternal (BrnArbStateCrashing.cpp ~556). Its collision policy is
# CollisionPolicyAttachedToVehicle, whose GenerateSceneQueries @0x82252690 / ProcessSceneQueryResults @0x82252888 the
# PC did not have (the empty base pair): on exe cb7f182f76df (L2's closure-on baseline, scratch/flow_run/
# l2won_h225_s80_r1) the fallback eye sat on the far side of the wall the car hit for its whole stretch -- 159
# occluded frames, a dark brick wall at point-blank range in the frame dumps.
#
# The witness is L2's BRN_CAMCOLLIDE_DIAG (b5 2596bf4f, NOT IN THE X360 BINARY): per published frame, the eye through
# a world-only 0.1 m sphere ("in"), the ground above / below, and the eye <-> car line tests ("occA" / "occB", OCC =
# either hits the world). The camera query chain runs by default, as on ARTIST.
# Wait for the junkyard outro to release the camera before firing, and pin the existing ultra-slow-motion
# harness scale. After profile delivery was restored, firing during the outro could leave only 31 fallback
# frames. These controls keep the full crash-camera witness and make closure A/B physics rows comparable.
#   powershell -ExecutionPolicy Bypass -File tools\tests\run_case.ps1 -Case b5-decomp\tests\L1CamCollideCrashLive.ps1 -Slot 1
@{
  Name    = 'l1_camcollide_crash'
  Area    = 'camera'
  Bug     = 'After a wall crash the fallback (chase) camera must pull in front of the wall between it and the car, not film the wall from behind (the owner''s "camera behind walls for crashes").'
  Frames  = $false
  Run     = @{
    Drive           = $true
    MaxSeconds      = 100
    CrashSweep      = '3261.11,-3.7,-1914.09'
    CrashSweepShots = '225:80'
    CrashSweepArm   = 4
  }
  DiagEnv = 'BRN_CRASH_RESPONSE_DIAG=1,BRN_CAMCOLLIDE_DIAG=1,BRN_CRASHCAM_DIAG=1,BRN_VLQ_DIAG=1,BRN_SWEEP_WAIT_ROAMING=1,BRN_ULTRA_SLOMO_SCALE=0.0075'
  Checks  = @(
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'the crash state ran'; Pattern = '\[crashcam\] container current state -> 2 \(ArbStateCrashing\)'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the crash fell back to the chase cam (moment type=-1)'; Pattern = '\[crashcam\] crash camera: moment type=-1 '; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the witness ran'; Pattern = '^\[camcol\] f='; Min = 100 }
    @{ Kind = 'Script'; Name = 'the fallback camera is not filmed through the wall (longest occluded stretch <= 2 frames)'; Script = {
        param($ctx)
        $in = $false; $n = 0; $occ = 0; $run = 0; $longest = 0; $first = $null
        foreach ($l in $ctx.LogLines) {
          if ($l -match '\[crashcam\] crash camera: moment type=-1 ') { $in = $true; continue }
          if ($l -match '\[crashcam\] crash camera: moment type=\d') { $in = $false; $run = 0; continue }
          if ($l -match '\[crashcam\] container current state -> (\d+)') { if ([int]$Matches[1] -ne 2) { $in = $false; $run = 0 }; continue }
          if ($in -and $l -match '^\[camcol\] f=(\d+) .* OCC=(\d)') {
            $n++
            if ($Matches[2] -eq '1') { $occ++; $run++; if ($null -eq $first) { $first = [int]$Matches[1] }; if ($run -gt $longest) { $longest = $run } }
            else { $run = 0 }
          }
        }
        @{ Pass = ($n -ge 60 -and $longest -le 2); Detail = "fallback-camera frames $n, occluded $occ, longest occluded stretch $longest$(if ($null -ne $first) { " (first at f=$first)" })" }
      } }
    @{ Kind = 'Script'; Name = 'report: the witness per arbitrator state (informational)'; Script = {
        param($ctx)
        $state = 'Boot'; $t = @{}; $order = @()
        foreach ($l in $ctx.LogLines) {
          if ($l -match '\[crashcam\] container current state -> -?\d+ \((\w+)\)') { $state = $Matches[1]; continue }
          if ($l -match '^\[camcol\] f=\d+ .* in=(-?\d) .* UNDER=(\d) OCC=(\d)') {
            if (-not $t.ContainsKey($state)) { $t[$state] = @{ f = 0; i = 0; u = 0; o = 0 }; $order += $state }
            $t[$state].f++; if ($Matches[1] -eq '1') { $t[$state].i++ }; if ($Matches[2] -eq '1') { $t[$state].u++ }; if ($Matches[3] -eq '1') { $t[$state].o++ }
          }
        }
        @{ Pass = $true; Detail = (($order | ForEach-Object { "$_ $($t[$_].f) (in $($t[$_].i), UNDER $($t[$_].u), OCC $($t[$_].o))" }) -join ' | ') }
      } }
    @{ Kind = 'LogCount';   Name = 'no volume line-kernel traps'; Pattern = '\[vlq\] TRAP'; Max = 0 }
  )
}
