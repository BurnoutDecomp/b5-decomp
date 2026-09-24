# FX-CAMRIG live case (crash-parity CC-4): the Showtime drive of ShowtimeContacts.ps1, with the witness that the
# crash-mode camera rig -- BehaviourAftertouchCrash::Update @0x82228158 -- is dispatched, follows the car and is the
# camera the director hands to the screen.
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxCamrigShowtime.ps1
# Diagnostics only print; the drive, the Showtime gesture and the inherited checks are unchanged.
#   BRN_CAMRIG_DIAG    `[camrig] Update frame N first F ... car x y z cam x y z dir .. dist D height H ...` -- the rig's
#                      own line (first ten frames after each Prepare, then every 15th): the lagged car origin it
#                      follows (PositionLag::Update's w row) and the camera position it produced this frame.
#   BRN_CRASHCAM_DIAG  the arbitrator's state edges: `container current state -> 4 (ArbStateCrashMode)` is the
#                      director handing the frame to the state that copies the rig's camera every frame.
#   BRN_CAMERA_TRACE   `[cam] f=N pos=x,y,z ...` -- the camera the director PUBLISHED that frame.
# Frames (every 30th present) land in <run dir>\frames.
#
# The follow bound in the third check is the transcription's own geometry, not a tuning: the eye is
# car + dir*dist + (0, manualHeight, 0) with |manualHeight| <= KF_MIN_CAMERA_MANUAL_HEIGHT_TWEAK (2.0, 0x82FAA710), then
# y += mfHeight; the smoothing only acts on a move shorter than sqrt(KF_SMOOTHING_STOP_DISTANCE_SQ) (5.0, 0x82FAA9C0)
# and takes KF_SMOOTHING_FACTOR (0.5, 0x82FAA7F0) of it, so the published point is < 2.5 m from the eye; the shakes are
# rotations (w untouched); the close-up blends toward target + flat*4 with target = car - 0.5 Y (<= 4.5 m away).
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'fxcamrig_showtime'
$case.Bug = 'Showtime crash-mode camera rig (BehaviourAftertouchCrash::Update) must run, follow the car and drive the published camera, with no assertions.'
$case.Frames = $true
$case.DiagEnv += ',BRN_CAMRIG_DIAG=1,BRN_CRASHCAM_DIAG=1,BRN_CAMERA_TRACE=1'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'director handed the frame to ArbStateCrashMode';
       Pattern = '\[crashcam\] container current state -> 4 \(ArbStateCrashMode\)'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'BehaviourAftertouchCrash::Update dispatched (first frame after Prepare)';
       Pattern = '\[camrig\] Update frame 0 first 1 '; Expect = $true }
    @{ Kind = 'Script'; Name = 'the rig follows the car: finite, moving, inside its own geometric envelope'; Script = {
        param($ctx)
        $inv = [Globalization.CultureInfo]::InvariantCulture
        $num = { param($s) $v = 0.0; if ([double]::TryParse($s, [Globalization.NumberStyles]::Float, $inv, [ref]$v)) { $v } else { [double]::NaN } }
        $rx = '\[camrig\] Update frame (\d+) first (\d) tempDebug (\d) manual (\d) car (\S+) (\S+) (\S+) cam (\S+) (\S+) (\S+) dir (\S+) (\S+) (\S+) dist (\S+) height (\S+) blend (\S+)'
        $rows = @()
        foreach ($l in $ctx.LogLines) {
            if ($l -match $rx) {
                $rows += ,@((& $num $Matches[5]), (& $num $Matches[6]), (& $num $Matches[7]),
                            (& $num $Matches[8]), (& $num $Matches[9]), (& $num $Matches[10]),
                            (& $num $Matches[14]), (& $num $Matches[15]))
            }
        }
        if ($rows.Count -lt 10) { return @{ Pass = $false; Detail = "$($rows.Count) [camrig] lines (want >= 10)" } }
        $bad = 0; $out = 0; $seps = @(); $camTravel = 0.0; $carTravel = 0.0
        $c0 = $rows[0]
        foreach ($r in $rows) {
            if (@($r | Where-Object { [double]::IsNaN($_) -or [double]::IsInfinity($_) }).Count -gt 0) { $bad++; continue }
            $sep = [math]::Sqrt(($r[3]-$r[0])*($r[3]-$r[0]) + ($r[4]-$r[1])*($r[4]-$r[1]) + ($r[5]-$r[2])*($r[5]-$r[2]))
            $seps += $sep
            if ($sep -gt ($r[6] + [math]::Abs($r[7]) + 4.5)) { $out++ }
            $dc = [math]::Sqrt(($r[3]-$c0[3])*($r[3]-$c0[3]) + ($r[4]-$c0[4])*($r[4]-$c0[4]) + ($r[5]-$c0[5])*($r[5]-$c0[5]))
            $dr = [math]::Sqrt(($r[0]-$c0[0])*($r[0]-$c0[0]) + ($r[1]-$c0[1])*($r[1]-$c0[1]) + ($r[2]-$c0[2])*($r[2]-$c0[2]))
            if ($dc -gt $camTravel) { $camTravel = $dc }
            if ($dr -gt $carTravel) { $carTravel = $dr }
        }
        $sorted = @($seps | Sort-Object)
        $median = if ($sorted.Count -gt 0) { $sorted[[int][math]::Floor($sorted.Count / 2)] } else { [double]::NaN }
        $pass = ($bad -eq 0) -and ($out -eq 0) -and ($camTravel -gt 1.0)
        @{ Pass = $pass; Detail = ("{0} lines, {1} non-finite, {2} outside dist+|height|+4.5; cam travelled {3:f1} m (car {4:f1} m); |cam-car| min {5:f2} median {6:f2} max {7:f2} m" -f
             $rows.Count, $bad, $out, $camTravel, $carTravel, $sorted[0], $median, $sorted[-1]) }
    } }
    @{ Kind = 'Script'; Name = 'the director published the rig''s camera (same-frame [cam] pose)'; Script = {
        param($ctx)
        $inv = [Globalization.CultureInfo]::InvariantCulture
        $num = { param($s) $v = 0.0; if ([double]::TryParse($s, [Globalization.NumberStyles]::Float, $inv, [ref]$v)) { $v } else { [double]::NaN } }
        $rigRx = '\[camrig\] Update frame \d+ first \d tempDebug \d manual \d car \S+ \S+ \S+ cam (\S+) (\S+) (\S+) '
        $camRx = '\[cam\] f=\d+ pos=([^,]+),([^,]+),(\S+) '
        $pending = $null; $pairs = 0; $match = 0; $d = @()
        foreach ($l in $ctx.LogLines) {
            if ($l -match $rigRx) { $pending = @((& $num $Matches[1]), (& $num $Matches[2]), (& $num $Matches[3])); continue }
            if ($null -ne $pending -and $l -match $camRx) {
                $p = @((& $num $Matches[1]), (& $num $Matches[2]), (& $num $Matches[3]))
                $dist = [math]::Sqrt(($p[0]-$pending[0])*($p[0]-$pending[0]) + ($p[1]-$pending[1])*($p[1]-$pending[1]) + ($p[2]-$pending[2])*($p[2]-$pending[2]))
                $d += $dist; $pairs++
                if ($dist -lt 0.01) { $match++ }
                $pending = $null
            }
        }
        if ($pairs -eq 0) { return @{ Pass = $false; Detail = 'no [camrig] line was followed by a published [cam] line' } }
        $sorted = @($d | Sort-Object)
        $median = $sorted[[int][math]::Floor($sorted.Count / 2)]
        @{ Pass = ($match * 2 -gt $pairs); Detail = ("{0} of {1} rig frames published within 1 cm of the rig's camera; |published-rig| median {2:f3} max {3:f3} m" -f
             $match, $pairs, $median, $sorted[-1]) }
    } }
)
$case
