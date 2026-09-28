# L4 boot order (conductor 2026-09-28, follow-up (2)) -- the saved car pose follows the junkyard the player was last
# in. BOOT 2 of 2: run it on the SAME slot right after tests/JunkyardPoseWriterLive.ps1, whose seed carried a pose that
# was never written ((0,0,0): it entered junkyard 312262, the one nearest the origin) and whose autosave ran after the
# junkyard exit. The pose ProgressionManager::PreWorldUpdate saved while the player was in the junkyard
# (lbIsInJunkyard = mJunkyardId != 0 || the online car select, PreWorldUpdate @0x823A5328 0x823A5B48..0x823A5B80)
# must come back: OnProfileLoaded @0x82397310 reads a non-zero pose at junkyard 312262's car and enters 312262 from it.
# On the pre-fix exe the pose stays (0,0,0) across any number of autosaves (L2's slot-2 chain l2rdyon -> l2rdyoff ->
# l2pinon -> l2pinoff, scratch/flow_run/*_h225_s80_r1).
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/JunkyardPoseWriterRebootLive.ps1 -Slot <n>

$KV_JUNKYARD_312262 = @(-381.583313, 915.523804)     # junkyard 312262's logged spawn[1] (x, z)

@{
  Name    = 'junkyard_pose_writer_reboot'
  Area    = 'flow'
  Bug     = 'After a boot spent in junkyard 312262 the saved car pose must be that junkyard car''s, not the never-written (0,0,0).'
  Frames  = $false
  Setup   = {
    param($ctx)
    if ($ctx.Slot -le 0) { throw 'JunkyardPoseWriterRebootLive requires -Slot greater than zero.' }
  }
  Run     = @{
    Drive      = $true
    SkipIntro  = $true
    AcceptGap  = 1.0
    MaxSeconds = 70
  }
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'Script';   Name = 'OnProfileLoaded reads the pose the junkyard wrote (non-zero, at junkyard 312262''s car) and enters 312262'; Script = {
        param($ctx)
        $line = $ctx.LogLines | Where-Object { $_ -match '^\[GameStateModule::OnProfileLoaded\] ' } | Select-Object -First 1
        if (-not $line) { return @{ Pass = $false; Detail = 'no OnProfileLoaded line' } }
        if ($line -notmatch 'junkyard=(\d+) .* savedPos=\((-?[0-9.]+), (-?[0-9.]+), (-?[0-9.]+)\)') { return @{ Pass = $false; Detail = $line } }
        $inv = [Globalization.CultureInfo]::InvariantCulture
        $jy = $Matches[1]; $x = [double]::Parse($Matches[2], $inv); $y = [double]::Parse($Matches[3], $inv); $z = [double]::Parse($Matches[4], $inv)
        $zero = ($x -eq 0.0 -and $y -eq 0.0 -and $z -eq 0.0)
        $d = [Math]::Sqrt([Math]::Pow($x - $KV_JUNKYARD_312262[0], 2) + [Math]::Pow($z - $KV_JUNKYARD_312262[1], 2))
        return @{ Pass = ((-not $zero) -and $d -lt 100.0 -and $jy -eq '312262')
                  Detail = ("junkyard={0} savedPos=({1}, {2}, {3}): {4:F1} m from 312262's spawn" -f $jy, $Matches[2], $Matches[3], $Matches[4], $d) }
      }.GetNewClosure() }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
