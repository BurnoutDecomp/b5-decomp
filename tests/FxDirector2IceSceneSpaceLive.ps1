# FX-DIRECTOR2 (crash parity 2026-09-25) live case: a scene-space ICE take projects through a LIVE scene space.
#
# MainDirector::Update @0x82274070 copies the published camera's transform into the director's ICE scene space
# (+0x12170) on every live frame whose camera is not in a scene-space shot (0x82274A28..0x82274A80, state flag 13).
# UpdateCameraBehavioursPostScene stages it into the frame's ICE::CameraSpaceHandler. So a take authored in
# eICE_SCENE_SPACE (43 of the 549 retail takes) is anchored to the last camera before it. The PC never wrote the matrix,
# so every such take projected through the ZERO matrix: its eye and look collapsed onto the world origin.
# The shot is the Stunt Run start (tools/tests/cases/stunt_run_lifecycle.ps1 / FxDirectorEndFlagsLive.ps1): stunt
# junction 480897 at (2641.5, 1.3, -1723.8) heading 169, event 558269, started through the harness's junction
# injection. The Stunt Run's intro take is Stunt_Intro, authored in scene space for ALL three intervals (eye and look);
# Race_Event_Start and Race_StartFX are others.
# Witness (NOT X360, BRN_CRASHCAM_DIAG): KeyAnimController::UpdateTransformationMatrix prints
#   [ice-scene] take guid G 'Name' eye space E look space L: scene origin (x, y, z) world eye (x, y, z)
# on the first frame of each scene-space stretch. The scene origin must be a real place (the last camera), not 0.
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxDirector2IceSceneSpaceLive.ps1
@{
  Name    = 'fxdirector2_ice_scene_space'
  Area    = 'director'
  Bug     = 'A scene-space ICE take (the Stunt Run''s Stunt_Intro) must project through the live scene space (the last published camera), not the zero matrix.'
  Frames  = $false
  Run     = @{
    Drive           = $true
    MotionProbe     = $true
    Teleport        = '2641.5,1.3,-1723.8,169'
    StartEvent      = $true
    EventFsm        = $true
    SkipTrainingTip = $true
    MaxSeconds      = 90
    SkipIntro       = $true
    AcceptGap       = 1.0
    SteerScript     = '0:none,22:left,38:right,54:left,70:right'
  }
  DiagEnv = 'BRN_CRASHCAM_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'the Stunt Run was started at the junction (BRN_START_EVENT injection)'
       Pattern = '\[start\] \*\*\*\*\* HARNESS-ONLY START INJECTION'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'a scene-space take played'; Pattern = '^\[ice-scene\] take guid \d+'; Min = 1 }
    @{ Kind = 'Script';     Name = 'every scene-space take projected through a live scene space (origin is not the world origin)'; Script = {
        param($ctx)
        $takes = @(); $zero = 0
        foreach ($l in $ctx.LogLines) {
          if ($l -match "^\[ice-scene\] take guid (?<g>\d+) '(?<n>[^']*)' eye space (?<e>\d+) look space (?<k>\d+): scene origin \((?<x>[-0-9.e]+), (?<y>[-0-9.e]+), (?<z>[-0-9.e]+)\)") {
            $x = [double]::Parse($Matches.x, [cultureinfo]::InvariantCulture)
            $y = [double]::Parse($Matches.y, [cultureinfo]::InvariantCulture)
            $z = [double]::Parse($Matches.z, [cultureinfo]::InvariantCulture)
            $r = [math]::Sqrt($x * $x + $y * $y + $z * $z)
            $takes += ('{0} ({1}) eye {2} look {3}: origin {4:0.0} m from 0' -f $Matches.n, $Matches.g, $Matches.e, $Matches.k, $r)
            if ($r -lt 1.0) { $zero++ }
          }
        }
        if ($takes.Count -eq 0) { return @{ Pass = $false; Detail = 'no [ice-scene] line' } }
        @{ Pass = ($zero -eq 0); Detail = (($takes | Select-Object -Unique) -join '; ') }
    } }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
