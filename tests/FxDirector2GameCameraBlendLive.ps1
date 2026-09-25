# FX-DIRECTOR2 (crash parity 2026-09-25) CC-14 live case: when a rival takes the player down, the "Takendown" ICE take
# keys CAMERA_BLEND_AMOUNT (retail [100,0,0,0,90,100]). MainDirector::Update @0x82274070 must then ease the frame camera
# through its CameraInterpolationController toward the shared chase camera (0x822749D4..0x82274A24) instead of cutting.
# Built on FxBridgesTakedownCamForcedLive.ps1 (BRN_FORCE_PLAYER_TAKEN_DOWN=15: the console's own force-takedown debug
# action with the roles swapped; the credit is forced, and the takedown chain and the director are the real code).
# Witnesses (NOT X360, BRN_CRASHCAM_DIAG, armed by RivalOrganic):
#   [takedown-cam] ArbStateCrashing: taken-down ICE camera started ...   the Takendown take plays
#   [gcblend] ON blend=B curve=C method=M camera->chase X m -> Y m ...   the controller ran on a keyed frame
#   [gcblend] OFF after N blended frame(s), peak P last L                 the keyed stretch ended
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxDirector2GameCameraBlendLive.ps1 --run-name fxdirector2_gcblend
$case = & (Join-Path $PSScriptRoot 'FxBridgesTakedownCamForcedLive.ps1')
$case.Name = 'fxdirector2_game_camera_blend'
$case.Bug = 'A blended ICE take (the player-taken-down "Takendown" take) must ease the frame camera toward the chase camera (MainDirector''s game-camera blend), not cut.'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'the Takendown ICE camera played (ArbStateCrashing)';
       Pattern = '\[takedown-cam\] ArbStateCrashing: taken-down ICE camera started'; Expect = $true }
    @{ Kind = 'LogCount'; Name = 'CC-14: a keyed game-camera blend ran the director''s interpolation controller'
       Pattern = '^\[gcblend\] ON blend=[0-9.]+'; Min = 1 }
    @{ Kind = 'Script'; Name = 'CC-14: the blend pulled the frame camera toward the chase camera, and consumed the blend'; Script = {
        param($ctx)
        $on = @(); $pulled = 0
        foreach ($l in $ctx.LogLines) {
            if ($l -match '^\[gcblend\] ON blend=(?<b>[-0-9.e]+) curve=(?<c>\d+) method=(?<m>\d+) camera->chase (?<x>[-0-9.e]+) m -> (?<y>[-0-9.e]+) m, blend left (?<r>[-0-9.e]+)') {
                $x = [double]::Parse($Matches.x, [cultureinfo]::InvariantCulture)
                $y = [double]::Parse($Matches.y, [cultureinfo]::InvariantCulture)
                $r = [double]::Parse($Matches.r, [cultureinfo]::InvariantCulture)
                $on += ('blend {0} curve {1} method {2}: {3:0.00} m -> {4:0.00} m (left {5})' -f $Matches.b, $Matches.c, $Matches.m, $x, $y, $r)
                if ($y -le $x -and $r -eq 0) { $pulled++ }
            }
        }
        if ($on.Count -eq 0) { return @{ Pass = $false; Detail = 'no [gcblend] ON line' } }
        @{ Pass = ($pulled -eq $on.Count); Detail = ($on -join '; ') }
    } }
)
$case
