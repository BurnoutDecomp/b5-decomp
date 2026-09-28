# L3 RACEINTRO (2026-09-27) live case: the race intro's countdown camera frames the start light, not the world origin.
#
# The owner's report: "The races intro doesn't show the traffic light going green, the camera is below the map".
# An offline race's countdown take is Race_Event_Start (guid 574883), eye / look spaces [SCENE, TRAFFIC_LIGHT, HEADING].
# Its middle interval is anchored in eICE_TRAFFIC_LIGHT_SPACE = GameState::mTrafficLightSpace, which only
# MainDirector::CalcTrafficLightSpace @0x8221A3A8 writes (ProcessInputQueue's countdown push, cases 30 / 47). The PC
# never called it: the space stayed at the identity and the eye sat at the world origin under the map for ~80 frames of
# the countdown, on the old exe 7fdddf649d45 and the owner's d65db9997047 alike
# (scratch/bugtest/runs/l3_raceintro_cam_ab/A_old_7fdddf649d45, B_owner_d65db9997047).
# The recipe is FxScenariosModeIntroRaceLive's offline race at junction 480886, with the camera witnesses.
# Witnesses (all NOT X360, all default-off):
#   [tl-space] CalcTrafficLightSpace box B controllers N instances M chose I at d2 D | space pos x,y,z fwd ... | player ...
#                                                   BRN_CAMERA_TRACE, one line per call (L3)
#   [cam] f=N pos=x,y,z fwd=...                     BRN_CAMERA_TRACE, the published camera per frame
#   [ice-prepare] guid G / [ice] Prepare guid G     the take bound (always-on first 8 / BRN_ICE_TRACE)
#   [motion] n N pos x y z                          -MotionProbe, the player car
#   [stunt] mode state -> E_GMS_*                   the mode rungs
# RED on d65db9997047 (evaluated with -NoRun on B_owner_d65db9997047): no [tl-space] line, the eye at the origin.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxRaceIntroTrafficLightSpaceLive.ps1 -Slot 4
@{
    Name = 'fxraceintro_trafficlight_space'
    Area = 'director'
    Bug = 'The race intro countdown take (Race_Event_Start 574883) must frame the start light through the traffic-light space CalcTrafficLightSpace computes, not the world origin under the map.'
    Frames = $true
    Run = @{
        Drive = $true
        Boost = ''
        MotionProbe = $true
        SkipIntro = $true
        AcceptGap = 1.0
        Teleport = '3003.9,6.6,-1675.6,0'
        StartEvent = $true
        EventFsm = $true
        SkipTrainingTip = $true
        MaxSeconds = 50
        FrameEvery = 20
    }
    DiagEnv = 'BRN_INTRO_TIMER_DIAG=1,BRN_CAMERA_TRACE=1,BRN_ICE_TRACE=1,BRN_CRASHCAM_DIAG=1'
    Checks = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
        @{ Kind = 'LogCount'; Name = 'the race intro ran (mode type 0)'; Pattern = '\[mode-intro\] IntroState::OnEnter mode type 0 '; Min = 1 }
        @{ Kind = 'LogCount'; Name = 'the countdown take Race_Event_Start (574883) was bound'; Pattern = 'Prepare guid 574883|\[ice-prepare\] guid 574883'; Min = 1 }
        @{ Kind = 'Script'; Name = 'CalcTrafficLightSpace ran on the countdown and took a start light at the junction (within 60 m of the player)'; Script = {
            param($ctx)
            $inv = [cultureinfo]::InvariantCulture
            $rows = @($ctx.LogLines | Where-Object { $_ -match '^\[tl-space\] CalcTrafficLightSpace ' })
            $good = @()
            foreach ($l in $rows) {
                if ($l -match 'chose (-?\d+) at d2 ([-\d.eE+]+) \| space pos ([-\d.eE+]+),([-\d.eE+]+),([-\d.eE+]+) .*\| player ([-\d.eE+]+),([-\d.eE+]+),([-\d.eE+]+)') {
                    $sx = [double]::Parse($Matches[3], $inv); $sy = [double]::Parse($Matches[4], $inv); $sz = [double]::Parse($Matches[5], $inv)
                    $px = [double]::Parse($Matches[6], $inv); $py = [double]::Parse($Matches[7], $inv); $pz = [double]::Parse($Matches[8], $inv)
                    $d = [math]::Sqrt(($sx - $px) * ($sx - $px) + ($sy - $py) * ($sy - $py) + ($sz - $pz) * ($sz - $pz))
                    if ([int]$Matches[1] -ge 0 -and $d -lt 60.0) { $good += ("instance {0}: space {1:0.0},{2:0.0},{3:0.0}, {4:0.0} m from the player, {5:0.00} m above it" -f $Matches[1], $sx, $sy, $sz, $d, ($sy - $py)) }
                }
            }
            @{ Pass = ($good.Count -gt 0); Detail = "$($rows.Count) [tl-space] line(s); at the junction: $(($good | Select-Object -First 3) -join ' | ')" }
        } }
        @{ Kind = 'Script'; Name = 'the countdown take never films the world origin, and its TRAFFIC_LIGHT interval frames the start light (eyes within 15 m of the [tl-space] origin)'; Script = {
            param($ctx)
            $inv = [cultureinfo]::InvariantCulture
            $car = $null; $space = $null; $inTake = $false; $n = 0; $far = @(); $origin = 0; $atLight = 0; $row = 0
            foreach ($l in $ctx.LogLines) {
                if ($l -match '^\[motion\] n \d+ pos ([-\d.eE+]+) ([-\d.eE+]+) ([-\d.eE+]+)') {
                    $car = @([double]::Parse($Matches[1], $inv), [double]::Parse($Matches[2], $inv), [double]::Parse($Matches[3], $inv)); continue
                }
                if ($l -match '^\[tl-space\] .*\| space pos ([-\d.eE+]+),([-\d.eE+]+),([-\d.eE+]+) ') {
                    $space = @([double]::Parse($Matches[1], $inv), [double]::Parse($Matches[2], $inv), [double]::Parse($Matches[3], $inv)); continue
                }
                if ($l -match 'Prepare guid 574883|\[ice-prepare\] guid 574883') { $inTake = $true; continue }
                if ($inTake -and $l -match '\[stunt\] mode state -> E_GMS_IN_PROGRESS') { break }
                if ($inTake -and $car -and $l -match '^\[cam\] f=(\d+) pos=([-\d.eE+]+),([-\d.eE+]+),([-\d.eE+]+) ') {
                    $x = [double]::Parse($Matches[2], $inv); $y = [double]::Parse($Matches[3], $inv); $z = [double]::Parse($Matches[4], $inv)
                    $d = [math]::Sqrt(($x - $car[0]) * ($x - $car[0]) + ($y - $car[1]) * ($y - $car[1]) + ($z - $car[2]) * ($z - $car[2]))
                    $o = [math]::Sqrt($x * $x + $y * $y + $z * $z)
                    $n++
                    if ($d -gt 150.0) { $far += ("row {0} (f={1}) {2:0.0} m" -f $n, $Matches[1], $d) }
                    if ($o -lt 200.0) { $origin++ }
                    if ($space) {
                        $s = [math]::Sqrt(($x - $space[0]) * ($x - $space[0]) + ($y - $space[1]) * ($y - $space[1]) + ($z - $space[2]) * ($z - $space[2]))
                        if ($s -lt 15.0) { $atLight++ }
                    }
                }
            }
            # Known and pre-existing (the same on 7fdddf649d45 and d65db9997047): the take's FIRST frame, a SCENE-interval
            # cut, lands ~430 m off for one frame. It is reported, not hidden: any other far row fails.
            $farOk = ($far.Count -eq 0) -or ($far.Count -eq 1 -and $far[0] -match '^row 1 ')
            @{ Pass = ($n -gt 60 -and $origin -eq 0 -and $atLight -ge 30 -and $farOk)
               Detail = ("{0} countdown-take camera rows; {1} within 200 m of the world origin; {2} within 15 m of the traffic-light space origin; more than 150 m from the car: {3}" -f $n, $origin, $atLight, $(if ($far.Count) { $far -join ', ' } else { 'none' })) }
        } }
        @{ Kind = 'Script'; Name = 'the race reaches E_GMS_IN_PROGRESS after the countdown'; Script = {
            param($ctx)
            $c = @($ctx.LogLines | Where-Object { $_ -match '\[stunt\] mode state -> E_GMS_IN_PROGRESS' }).Count
            @{ Pass = ($c -gt 0); Detail = "$c E_GMS_IN_PROGRESS rung(s)" }
        } }
    )
}
