# FX-LASTFIX item 1 (crash parity 2026-09-26) live case: BehaviourIceAnim's heading space eases 20% a frame.
#
# BehaviourIceAnim::Update @0x82247108 blends its heading space (this+0x610) towards the look-at of its secondary
# vehicle's flattened heading with rw::math::vpu::SLerp @0x82216858 (bl 0x82247354), amount 0.2: the class-static
# VecFloat KF_HEADING_SPACE_2_SLERP_AMOUNT, which the CRT thunk 0x82C49580 splats from flt_82004744. The PC called a
# pointer-amount overload whose link stub returned `to`, with 1.0f: the space snapped to the look-at every frame.
# The one retail take that reads that space (eICE_HEADING2_SPACE, 12) is Takedown_ICE_Shut (guid 554362, eye / look
# [8, 12, 8] -- scratch/CRASHPARITY_0922/chaincheck2/icetakes.txt), the shutdown takedown's shot, which
# FxDirector2ShutdownCamLive.ps1 plays (BRN_ALWAYS_SHUTDOWN_CAM=1, the console's "Always do shutdown TD camera" toggle;
# the AI pad in pursuit). NOTE: the space reaches that take's camera only with FX-LASTFIX item 1b (the four
# CameraSpaceHandler writes); this case witnesses the easing itself.
# Witness (NOT X360, BRN_CRASHCAM_DIAG, armed by RivalOrganic): per take, frames 0..5, 96 lines a run at most:
#   [iceanim] heading2 ease take <guid> '<name>' frame N: <before> -> <after> deg (slerp remaining <r> deg), eye space E look space L
# "before" / "after" are the angles between the heading space's forward and the look-at's around the SLerp. The console's
# words (run on emu64: run_fxlastfix_ice_heading_slerp.py) give after == 0.8 x before on every frame whose look-at moved
# (the arc arm exactly; the lerp arm under 2 degrees within a hair). The old stub gave 0, and printed nothing.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxLastfixIceHeadingEaseLive.ps1 --ai-pad pursuit --no-frames --run-name fxlastfix_heading_ease
$case = & (Join-Path $PSScriptRoot 'FxDirector2ShutdownCamLive.ps1')
$case.Name = 'fxlastfix_heading_ease'
$case.Bug = 'BehaviourIceAnim''s heading space (eICE_HEADING2_SPACE, read by Takedown_ICE_Shut) must ease 20% a frame towards its look-at, as the console''s SLerp with the 0.2 splat does, not snap to it.'
$case.Checks += @(
    @{ Kind = 'LogCount'; Name = 'item 1: the heading-space witness fired on Takedown_ICE_Shut (guid 554362)'; Pattern = '^\[iceanim\] heading2 ease take 554362 '; Min = 1 }
    @{ Kind = 'Script'; Name = 'item 1: every take frame whose look-at moved eases 20% (after / before in [0.7, 0.9] when before >= 0.5 deg)'; Script = {
        param($ctx)
        $inv = [cultureinfo]::InvariantCulture
        $rows = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '^\[iceanim\] heading2 ease take (-?\d+) ''([^'']*)'' frame (\d+): ([-\d.]+) -> ([-\d.]+) deg \(slerp remaining ([-\d.]+) deg\)') {
                [pscustomobject]@{ Guid = $Matches[1]; Take = $Matches[2]; Frame = [int]$Matches[3]
                                   Before = [double]::Parse($Matches[4], $inv); After = [double]::Parse($Matches[5], $inv)
                                   Remaining = [double]::Parse($Matches[6], $inv) }
            }
        })
        $moved = @($rows | Where-Object { $_.Before -ge 0.5 })
        $bad = @($moved | Where-Object { ($_.After / $_.Before) -lt 0.7 -or ($_.After / $_.Before) -gt 0.9 })
        $shown = @($moved | Select-Object -First 10 | ForEach-Object {
            [string]::Format($inv, "{0} f{1}: {2:F3} -> {3:F3} ({4:F3}x, remaining {5:F3})", $_.Take, $_.Frame, $_.Before,
                             $_.After, ($_.After / $_.Before), $_.Remaining) })
        @{ Pass = ($moved.Count -gt 0 -and $bad.Count -eq 0)
           Detail = ("{0} witness line(s), {1} with a moved look-at, {2} outside [0.7, 0.9]: {3}" -f
                     $rows.Count, $moved.Count, $bad.Count, ($shown -join ' | ')) }
    } }
)
$case
