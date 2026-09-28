# L4 boot order (conductor 2026-09-28, follow-up (2)) -- the saved car pose follows the junkyard the player was last
# in. BOOT 1 of 2: boot a save whose pose was never written (a PC-created save: Profile::Construct's (0,0,0)), let
# the player sit in the junkyard OnProfileLoaded enters, drive out, and let the autosave write the profile. BOOT 2 is
# tests/JunkyardPoseWriterRebootLive.ps1 on the SAME slot: its OnProfileLoaded must read the junkyard car's pose back.
#
# THE CONSOLE. GameStateModule::PreWorldUpdate @0x823A5328 hands ProgressionManager::PreWorldUpdate
# lbIsInJunkyard = (mCarSelectManager.mJunkyardId != 0) || mOnlineCarSelectManager.mbIsInOnlineCarSelect
# (0x823A5B48..0x823A5B80: `ld r11, 0(this+0x2CDC0)`, else `lbzx r11, r31, 0x2CE34`). While it holds, the callee's
# player-car arm stores the car's transform into Profile+0x30 / +0x40 (`lvx128 v0, rcs, 0x220 ; stvx128 v0, pm,
# 0x1A0`), which the autosave writes and the next boot's OnProfileLoaded @0x82397310 enters the nearest junkyard of.
# THE PC passed a constant false, so no save ever got its pose updated: a PC-created save kept (0,0,0) and booted into
# junkyard 312262 (nearest the origin) forever -- L2's slot-2 runs l2rdyon -> l2rdyoff -> l2pinon -> l2pinoff
# (scratch/flow_run/*_h225_s80_r1, each autosaving) all load savedPos=(0,0,0).
#
# The seed: a COPY of L2's PC-written slot-2 save (scratch/OWNERLIST_0927/L4/profile/zero_pose_seed_21db22a8.sav,
# itself a copy of scratch/OWNERLIST_0927/L2/slot2_profile_drifted_21db22a8.sav), copied into THIS slot by Setup.
# Never slot 0. NO FRAME DUMP.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/JunkyardPoseWriterLive.ps1 -Slot <n>

$lProfileSource = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'scratch\OWNERLIST_0927\L4\profile\zero_pose_seed_21db22a8.sav'

$FirstLine = {
  param($lines, $pattern, $from)
  for ($n = [Math]::Max(0, $from); $n -lt $lines.Count; $n++) { if ($lines[$n] -match $pattern) { return $n } }
  return -1
}

$lSetup = {
  param($ctx)
  if ($ctx.Slot -le 0) {
    throw 'JunkyardPoseWriterLive requires -Slot greater than zero.'
  }
  if (-not (Test-Path $lProfileSource)) {
    throw "Missing zero-pose test seed: $lProfileSource"
  }
  $lDst = Join-Path $ctx.Root ("build\game\Memcard_" + $ctx.Slot)
  New-Item -ItemType Directory -Force $lDst | Out-Null
  if (Test-Path (Join-Path $lDst 'Profile.sav')) {
    Copy-Item (Join-Path $lDst 'Profile.sav') (Join-Path $ctx.RunDir 'Profile.before.sav')
  }
  Copy-Item $lProfileSource (Join-Path $lDst 'Profile.sav') -Force
  Write-Host "[case] Setup: seeded $lDst\Profile.sav from a copy of $lProfileSource"
}.GetNewClosure()

@{
  Name    = 'junkyard_pose_writer'
  Area    = 'flow'
  Bug     = 'While the player is in a junkyard the profile''s car pose must follow the car (ProgressionManager::PreWorldUpdate''s lbIsInJunkyard), so the autosave writes the junkyard the player was last in.'
  Frames  = $false
  Setup   = $lSetup
  Run     = @{
    Drive      = $true
    SkipIntro  = $true
    AcceptGap  = 1.0
    MaxSeconds = 100
  }
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the seed''s pose was never written: OnProfileLoaded enters 312262, the junkyard nearest the origin'
       Pattern = '^\[GameStateModule::OnProfileLoaded\] junkyard=312262 .* savedPos=\(0\.000000, 0\.000000, 0\.000000\)'; Min = 1; Max = 1 }
    @{ Kind = 'Script';   Name = 'the autosave writes the profile after the junkyard exit (so the pose it saves is the junkyard car''s)'; Script = {
        param($ctx)
        $lines = $ctx.LogLines
        $exit = & $FirstLine $lines '^=== CarSelectManager: Exit state is finished' 0
        if ($exit -lt 0) { return @{ Pass = $false; Detail = 'no junkyard exit' } }
        $written = & $FirstLine $lines '^\[SaveLoadPC\] profile container WRITTEN' $exit
        return @{ Pass = ($written -ge 0); Detail = ("junkyard exit at log line {0}; profile written at {1}" -f ($exit + 1), ($written + 1)) }
      }.GetNewClosure() }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
