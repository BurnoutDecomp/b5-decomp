# OWNERLIST 2026-09-27, lane L5 MENUS, part A -- THE PAUSE CAMERA AFTER A CRASH (the crash-nav fly-by).
#
# ArbStateCrashNav::Update @0x8226DC98 drives the pause frame from the road-runner fly-by (BehaviourRoadRunner) instead of
# the ICE pause playlist while the player car's mbStartedDeforming (RaceCarState +0x44D) is set, i.e. after any crash.
# BehaviourRoadRunner::Update @0x82247E98 seats the fly-by on the lane nearest the player car (shared info +0x280,
# 0x82247F14) and walks it along the lane; the arbitrator turns it about past 400 m (OUTER, dyn-init 0x82C48488).
# On the PC the fly-by was seeded at the world ORIGIN and its backward walk (@0x8222B100) had lost the 0.0001 guards, so
# the first crashed-car pause flew a road across the city and fired 2.05 million "Current rung got out of sync" asserts
# (scratch/bugtest/runs/menus_pause_camera_driven/20260928_073346).
#
# The run: a returning boot on slot 6, drive (the harness's accel/steer schedule scrapes the car), open the Driver
# Details pause at DRIVING+30 s, back out at DRIVING+50 s. The log is capped at 64 MB (an assert storm aborts the run).
# Witnesses: BRN_CRASHCAM_DIAG [pause-cam] lines (the sample line's flyby flag, eyes and carDist), BRN_PFX_DIAG [pfx].
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/MenusPauseCameraCrashedLive.ps1 -Slot 6
@{
  Name    = 'menus_pause_camera_crashed'
  Area    = 'gui'
  Bug     = 'Pausing after a crash must fly the crash-nav road-runner camera along the road at the player car; the PC seeded it at the world origin and its backward lane walk stormed asserts.'
  Frames  = $true
  Run     = @{
    SkipIntro    = $true
    AcceptGap    = 1.0
    MaxSeconds   = 120
    MaxLogMB     = 64
    Drive        = $true
    MotionProbe  = $true
    PauseAt      = '30'
    PauseTarget  = 'driver'
    UnpauseAt    = '50'
    FrameEvery   = 60
  }
  DiagEnv = 'BRN_SCREEN_DIAG=1,BRN_CRASHCAM_DIAG=1,BRN_PFX_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the Driver Details pause screen opened (CN_D_DETAIL)'; Pattern = "\[screen\] ENTER 'CN_D_DETAIL\s*'"; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the arbitrator entered the crash-nav ICE camera state on the pause (0x8226B0A0..0x8226B100)'
       Pattern = '\[pause-cam\] arbitrator NORMAL -> CRASH_NAV_ICE_CAMERAS \(crashNavShown 1'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'ArbStateCrashNav::Prepare set up the pause playlist (@0x822660A8)'
       Pattern = '\[pause-cam\] ArbStateCrashNav::Prepare INACTIVE -> PREPARING .*pause playlist [1-9]'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the state went ACTIVE (-> 3, @0x8226DD20; Prepare and the first Update share one frame)'
       Pattern = '\[pause-cam\] ArbStateCrashNav state [01] -> 3'; Min = 1 }
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
    @{ Kind = 'Script';   Name = 'the crashed-car pause is flown by the road-runner at the car (every fly-by sample within the 400 m OUTER gate)'; Script = {
        param($ctx)
        $laD = @()
        foreach ($line in $ctx.LogLines) {
            # t 0 is the Prepare frame: the fly-by is allocated that frame and produces its first camera on the next.
            if ($line -match '\[pause-cam\] sample t (\S+) flyby 1 .* carDist (\S+)' -and [double]$Matches[1] -gt 0.0) { $laD += [double]$Matches[2] }
        }
        if ($laD.Count -lt 5) { return @{ Pass = $false; Detail = ("{0} fly-by samples (need >= 5: the car must have crashed before the pause)" -f $laD.Count) } }
        $lfMax = ($laD | Measure-Object -Maximum).Maximum
        @{ Pass = ($lfMax -lt 400.0); Detail = ("{0} fly-by samples: first {1:f1} m, max {2:f1} m from the car" -f $laD.Count, $laD[0], $lfMax) }
    } }
    @{ Kind = 'LogCount'; Name = 'no lane-walk "Current rung got out of sync" assert'; Pattern = 'Current rung got out of sync'; Max = 0 }
    @{ Kind = 'Script';   Name = 'evidence: world-region frame motion (not a gate: the pause takes are slow dollies)'; Script = {
        param($ctx)
        $lsPy = Join-Path (Split-Path $ctx.Case.CasePathForFrames -Parent) 'menus_pause_frames.py'
        $lsOut = & py -3 $lsPy $ctx.RunDir 2>&1
        try { $lR = ("$lsOut" | ConvertFrom-Json) } catch { return @{ Pass = $true; Detail = "menus_pause_frames.py: $lsOut" } }
        if (-not $lR.pause_window) { return @{ Pass = $true; Detail = "no paused frames: $lsOut" } }
        @{ Pass = $true; Detail = ("paused motion {0:f2} over {1} frames (running before: {2:f2})" -f $lR.pause_window.motion_mean,
                                   $lR.pause_window.frames, $lR.before_window.motion_mean) }
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
