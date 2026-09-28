# OWNERLIST 2026-09-27, lane L5 MENUS, part A -- THE CRASH-NAV FLY-BY TURNS ABOUT (the backward lane walk, live).
#
# The crashed-car pause (MenusPauseCameraCrashedLive.ps1) proves the fly-by is seated at the car. This case holds the pause
# long enough for the fly-by to reach the OUTER gate, which is what runs the backward lane walk:
#   - ArbStateCrashNav::Update @0x8226DC98, state 3 (ACTIVE): the camera is more than 400 m from the car after 2 s in the
#     state (OUTER 160000, dyn-init 0x82C48488; flt_82001D9C == 2.0) -> "Black_In_BW", state 4 (ACTIVE_TURNABOUT);
#   - state 4, after 1 s (flt_82001C98): stb 0 to the fly-by's prepared byte and BehaviourRoadRunner::Reverse
#     (asm @0x8226DF7C), which negates the direction, the desired speed and the truck's speed. The fly-by re-seats at the
#     car and flies at -3 m/s (flt_82004270 == 3.0), so TrafficLaneTruck::MoveAlongTrafficLane @0x8222BC48 dispatches
#     MoveAlongTrafficLaneBackwards @0x8222B100 -- the walk that lost its 0.0001 guards and fired 2.05 million
#     "Current rung got out of sync" asserts (scratch/bugtest/runs/menus_pause_camera_driven/20260928_073346);
#   - back inside 350 m (INNER 122500, dyn-init 0x82C484B0) after 2 s: state 3 again, still walking backward.
# The fly-by covers about 3 m per second, so the OUTER gate needs a pause of two to three minutes.
#
# The run: a returning boot on slot 6, drive (the harness's accel/steer schedule scrapes the car, which sets
# mbStartedDeforming), open the Driver Details pause at DRIVING+30 s, and back out at DRIVING+330 s. The log is capped
# at 64 MB (an assert storm aborts the run).
# Witnesses:
#   - BRN_CRASHCAM_DIAG: the [pause-cam] state lines, and the sample line's carDist;
#   - the [flyby] line's truck speed (its sign is the walk direction);
#   - BRN_PFX_DIAG: the [pfx] lines.
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/MenusPauseCameraTurnaboutLive.ps1 -Slot 6
@{
  Name    = 'menus_pause_camera_turnabout'
  Area    = 'gui'
  Bug     = 'A long pause after a crash must turn the crash-nav fly-by about at 400 m and walk it backward along the road; the PC backward lane walk stormed "Current rung got out of sync" asserts.'
  Frames  = $true
  Run     = @{
    SkipIntro    = $true
    AcceptGap    = 1.0
    MaxSeconds   = 480
    MaxLogMB     = 64
    Drive        = $true
    MotionProbe  = $true
    PauseAt      = '30'
    PauseTarget  = 'driver'
    UnpauseAt    = '330'
    FrameEvery   = 600
  }
  DiagEnv = 'BRN_SCREEN_DIAG=1,BRN_CRASHCAM_DIAG=1,BRN_PFX_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the Driver Details pause screen opened (CN_D_DETAIL)'; Pattern = "\[screen\] ENTER 'CN_D_DETAIL\s*'"; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the arbitrator entered the crash-nav state on the pause (0x8226B0A0..0x8226B100)'
       Pattern = '\[pause-cam\] arbitrator NORMAL -> CRASH_NAV_ICE_CAMERAS \(crashNavShown 1'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the state went ACTIVE (-> 3, @0x8226DD20)'
       Pattern = '\[pause-cam\] ArbStateCrashNav state [01] -> 3'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the fly-by reached the OUTER gate and the state turned about (3 -> 4)'
       Pattern = '\[pause-cam\] ArbStateCrashNav state 3 -> 4'; Min = 1 }
    @{ Kind = 'Script';   Name = 'after the turnabout the truck walks BACKWARD (negative speed: MoveAlongTrafficLaneBackwards @0x8222B100 is dispatched)'; Script = {
        param($ctx)
        $lbTurned = $false; $liBefore = 0; $liNegative = 0; $lsFirst = ''
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[pause-cam\] ArbStateCrashNav state 3 -> 4') { $lbTurned = $true }
            if ($line -match '\[flyby\] f\d+ dt \S+ speed (\S+)') {
                $lfSpeed = [double]$Matches[1]
                if (-not $lbTurned) { ++$liBefore }
                elseif ($lfSpeed -lt 0.0) { if ($liNegative -eq 0) { $lsFirst = $Matches[1] }; ++$liNegative }
            }
        }
        @{ Pass = ($lbTurned -and $liNegative -ge 5);
           Detail = ("turned about: {0}; [flyby] samples before it: {1}; negative-speed samples after it: {2} (first speed {3})" -f $lbTurned, $liBefore, $liNegative, $lsFirst) }
    } }
    @{ Kind = 'LogCount'; Name = 'the turned-about fly-by came back inside the INNER gate (4 -> 3)'
       Pattern = '\[pause-cam\] ArbStateCrashNav state 4 -> 3'; Min = 1 }
    @{ Kind = 'Script';   Name = 'the OUTER gate holds the fly-by near the car (every ACTIVE fly-by sample within 410 m)'; Script = {
        param($ctx)
        $laD = @()
        foreach ($line in $ctx.LogLines) {
            # t 0 is the Prepare frame: the fly-by is allocated that frame and produces its first camera on the next.
            if ($line -match '\[pause-cam\] sample t (\S+) flyby 1 .* carDist (\S+)' -and [double]$Matches[1] -gt 0.0) { $laD += [double]$Matches[2] }
        }
        if ($laD.Count -lt 5) { return @{ Pass = $false; Detail = ("{0} fly-by samples (need >= 5: the car must have crashed before the pause)" -f $laD.Count) } }
        $lfMax = ($laD | Measure-Object -Maximum).Maximum
        @{ Pass = ($lfMax -lt 410.0); Detail = ("{0} fly-by samples: first {1:f1} m, max {2:f1} m, last {3:f1} m from the car" -f $laD.Count, $laD[0], $lfMax, $laD[-1]) }
    } }
    @{ Kind = 'LogCount'; Name = 'no lane-walk "Current rung got out of sync" assert'; Pattern = 'Current rung got out of sync'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'a black-and-white hook reached the GUI effects arbitrator'
       Pattern = "\[pfx\] StartHook '(Black_Out_BW|Black_In_BW)'"; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the arbitrator came back to NORMAL after the unpause (case 4 exit, 0x8226B3B8)'
       Pattern = '\[pause-cam\] arbitrator CRASH_NAV_ICE_CAMERAS -> NORMAL \(crashNavShown 0'; Min = 1 }
    @{ Kind = 'Script';   Name = 'the world behind the pause is black and white (median world-region saturation <= 0.05)'; Script = {
        param($ctx)
        $lsPy = Join-Path (Split-Path $ctx.Case.CasePathForFrames -Parent) 'menus_pause_frames.py'
        $lsOut = & py -3 $lsPy $ctx.RunDir 2>&1
        try { $lR = ("$lsOut" | ConvertFrom-Json) } catch { return @{ Pass = $false; Detail = "menus_pause_frames.py: $lsOut" } }
        if (-not $lR.pause_window) { return @{ Pass = $false; Detail = "no paused frames: $lsOut" } }
        @{ Pass = ($lR.pause_window.sat_median -le 0.05);
           Detail = ("paused {0} frames {1}..{2}: sat {3:f4} motion {4:f2}; running before: sat {5:f4} motion {6:f2}" -f
                     $lR.pause_window.frames, $lR.pause_window.first, $lR.pause_window.last, $lR.pause_window.sat_median,
                     $lR.pause_window.motion_mean, $lR.before_window.sat_median, $lR.before_window.motion_mean) }
    } }
    @{ Kind = 'Script';   Name = 'evidence: the [pause-cam] state transitions in order'; Script = {
        param($ctx)
        $laT = @()
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[pause-cam\] ArbStateCrashNav state (\d+ -> \d+) \(time ([0-9.]+)') { $laT += ("{0} @{1:f1}s" -f $Matches[1], [double]$Matches[2]) }
        }
        @{ Pass = $true; Detail = $(if ($laT.Count) { ($laT | Select-Object -First 12) -join '; ' } else { 'none' }) }
    } }
    @{ Kind = 'Script';   Name = 'evidence: StartHook / bridge 495 requests by hook name'; Script = {
        param($ctx)
        $counts = @{}
        foreach ($line in $ctx.LogLines) {
            if ($line -match "\[pfx\] StartHook '([^']*)'") { $counts['gui:' + $Matches[1]] = 1 + [int]$counts['gui:' + $Matches[1]] }
            if ($line -match "\[pfx-dir\] bridge posts 495 start '([^']*)'") { $counts['495:' + $Matches[1]] = 1 + [int]$counts['495:' + $Matches[1]] }
        }
        $summary = ($counts.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name) x$($_.Value)" }) -join ', '
        @{ Pass = $true; Detail = ("hooks: {0}" -f $(if ($summary) { $summary } else { 'none' })) }
    } }
    @{ Kind = 'Script';   Name = 'no NEW assert family while the pause camera runs (CN_D_DETAIL enter .. the arbitrator back to NORMAL)'; Script = {
        param($ctx)
        # Windowed on purpose: the boot of this shared tree can carry other lanes' in-flight asserts; this case owns the pause.
        $laKnown = @(Get-Content (Join-Path (Split-Path $ctx.Case.CasePathForFrames -Parent) 'MenusKnownAsserts.txt') |
                     Where-Object { $_.Trim() -ne '' -and -not $_.StartsWith('#') })
        $lbIn = $false; $laNew = @(); $liKnown = 0
        foreach ($line in $ctx.LogLines) {
            if ($line -match "\[screen\] ENTER 'CN_D_DETAIL") { $lbIn = $true }
            if ($lbIn -and $line -match '^\[ASSERT \d+\]\s*(.*)$') {
                $lsMsg = ($Matches[1] -replace '\s*\[repeat.*$', '') -replace '\s*\([^()]*:\d+\)\s*$', ''
                $lbK = $false; foreach ($re in $laKnown) { if ($lsMsg -match $re) { $lbK = $true; break } }
                if ($lbK) { ++$liKnown } else { $laNew += $lsMsg }
            }
            if ($line -match '\[pause-cam\] arbitrator CRASH_NAV_ICE_CAMERAS -> NORMAL') { $lbIn = $false }
        }
        @{ Pass = ($laNew.Count -eq 0); Detail = ("in the pause window: {0} known assert line(s), {1} new: {2}" -f $liKnown, $laNew.Count, (($laNew | Select-Object -First 4) -join ' | ')) }
    } }
    @{ Kind = 'Script';   Name = 'evidence: every assert family in the run (not a gate: the boot carries other lanes'' WIP)'; Script = {
        param($ctx)
        $lFam = @{}
        foreach ($line in $ctx.LogLines) {
            if ($line -match '^\[ASSERT \d+\]\s*(.*)$') {
                $lsMsg = ($Matches[1] -replace '\s*\[repeat.*$', '') -replace '\s*\([^()]*:\d+\)\s*$', ''
                $lFam[$lsMsg] = 1 + [int]$lFam[$lsMsg]
            }
        }
        $lsList = ($lFam.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Value)x '$($_.Name.Substring(0, [Math]::Min(60, $_.Name.Length)))'" }) -join '; '
        @{ Pass = $true; Detail = $(if ($lsList) { $lsList } else { 'none' }) }
    } }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
  CasePathForFrames = $PSCommandPath
}
