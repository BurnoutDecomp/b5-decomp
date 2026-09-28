# OWNERLIST 2026-09-27, lane L5 MENUS, part A -- THE PAUSE CAMERA.
#
# The owner: "The camera behind the pause menu isn't the correct pause camera with black and white filter".
#
# The console chain (ARTIST):
#   START in free roam -> CN_D_DETAIL -> CrashNavDriverDetails::OnEnter @0x824CEC80 posts {8, 191, 12, 0, 0}
#   -> BridgeGuiToDirector case 191 payload 0 -> InputBuffer::SetGotCrashNavShownEvent
#   -> MainDirector::PostGuiUpdate @0x82236F88 -> GameState::mbCrashNavShown (+0x101)
#   -> Arbitrator::Update @0x8226ADA0, NORMAL state (0x8226B0A0..0x8226B100):
#        if ((GameState+0x101 || +0x1B0) && !+0xD9) { mArbStateCrashNav Prepare (vtable +4), Update (vtable +8); meState = 4 }
#   -> state 4 (0x8226B358..0x8226B3BC): ArbStateCrashNav::Update, frame camera = its camera; back to NORMAL once the
#      state has released itself (ArbStateCrashNav::meState == 0).
#   -> ArbStateCrashNav::Prepare @0x822660A8 loops the shared PAUSE playlist through its ICE movie player (the moving
#      camera); ArbStateCrashNav::Update @0x8226DC98 requests "Black_In_BW" / "Black_Out_BW" every frame
#      (EnsureEffectIsPlaying) -> BridgeDirectorToGui -> GUI 495 -> BrnGui::EffectsArbitrator::StartHook (the
#      black-and-white world).
#   The PC carried the arbitrator's trigger and both exits as comments: every pause kept the frozen gameplay camera in
#   full colour.
#
# The run: a returning boot on slot 6 (the car is NOT driven, so the pause cannot land on a crash and its crash
# desaturation), open the Driver Details pause at DRIVING+20 s, back out at DRIVING+40 s.
# Witnesses:
#   BRN_SCREEN_DIAG   `[screen] ENTER 'CN_D_DETAIL '`   -- the pause screen opened
#   BRN_CRASHCAM_DIAG `[pause-cam] ...`                 -- the arbitrator's CRASH_NAV edges and ArbStateCrashNav's own
#   BRN_PFX_DIAG      `[pfx-dir] bridge posts 495 start '<hook>'` / `[pfx] StartHook '<hook>'`
#   frames (one per 60 presents), measured by menus_pause_frames.py over the world regions the pause UI leaves bare.
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/MenusPauseCameraLive.ps1 -Slot 6
@{
  Name    = 'menus_pause_camera'
  Area    = 'gui'
  Bug     = 'Pausing (Driver Details) must switch the director to ArbStateCrashNav (the moving pause-playlist camera) and request the black-and-white hooks; the PC kept the frozen gameplay camera in full colour.'
  Frames  = $true
  Run     = @{
    SkipIntro    = $true
    AcceptGap    = 1.0
    MaxSeconds   = 110
    PauseAt      = '20'
    PauseTarget  = 'driver'
    UnpauseAt    = '40'
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
    @{ Kind = 'Script';   Name = 'the pause camera is the ICE movie camera and it moves (the take advances, the eye travels >= 1 m)'; Script = {
        param($ctx)
        # [pause-cam] sample lines (BRN_CRASHCAM_DIAG, every 30th ACTIVE frame): the ICE take's playback position,
        # the ICE camera's eye and the state camera's eye. The world-region frame delta is too blunt for the pause
        # takes -- slow dollies of ~0.2 m/s -- so the move is measured on the camera itself.
        $laS = @()
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[pause-cam\] sample t (\S+) flyby (\d) inInterp (\d) outInterp (\d) wrapperPlaying (\d) movieValid (\d) pos (\S+) iceEye \(([^,]+), ([^,]+), ([^)]+)\) stateEye \(([^,]+), ([^,]+), ([^)]+)\)') {
                $laS += ,@([double]$Matches[1], [int]$Matches[3], [int]$Matches[5], [double]$Matches[7],
                           [double]$Matches[8], [double]$Matches[9], [double]$Matches[10],
                           [double]$Matches[11], [double]$Matches[12], [double]$Matches[13]) }
        }
        $laLive = @($laS | Where-Object { $_[1] -eq 0 -and $_[2] -eq 1 })
        if ($laLive.Count -lt 5) { return @{ Pass = $false; Detail = ("{0} ICE samples (need >= 5; {1} lines in all)" -f $laLive.Count, $laS.Count) } }
        $lfMax = 0.0; $liMismatch = 0; $liAdvances = 0
        for ($i = 0; $i -lt $laLive.Count; ++$i) {
            $a = $laLive[$i]
            if ([Math]::Abs($a[4] - $a[7]) + [Math]::Abs($a[5] - $a[8]) + [Math]::Abs($a[6] - $a[9]) -gt 1e-3) { ++$liMismatch }
            if ($i -gt 0 -and $a[3] -gt $laLive[$i - 1][3]) { ++$liAdvances }
            for ($j = 0; $j -lt $i; ++$j) {
                $b = $laLive[$j]
                $d = [Math]::Sqrt(($a[4]-$b[4])*($a[4]-$b[4]) + ($a[5]-$b[5])*($a[5]-$b[5]) + ($a[6]-$b[6])*($a[6]-$b[6]))
                if ($d -gt $lfMax) { $lfMax = $d }
            }
        }
        @{ Pass = ($lfMax -ge 1.0 -and $liMismatch -eq 0 -and $liAdvances -ge ($laLive.Count / 2));
           Detail = ("{0} samples: the ICE eye travelled up to {1:f2} m; the take advanced on {2} of {3} steps; state camera != ICE camera on {4}" -f
                     $laLive.Count, $lfMax, $liAdvances, ($laLive.Count - 1), $liMismatch) }
    } }
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
