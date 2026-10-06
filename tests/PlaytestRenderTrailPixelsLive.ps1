# Native before/after readbacks bracket only the unchanged ORIGINAL trail pass.
# A fixed published world quad is tracked through actual simulation seconds.
$case = & (Join-Path $PSScriptRoot 'PlaytestRenderReflectTrailLive.ps1')
$case.Name = 'playtest_render_trail_pixels_1005'
$case.Bug = 'Measure visible original trail RGB coverage/fade and valid post-expiry zero coverage on a fixed world quad.'
$case.Run.MinFreeGB = 20 # conductor-approved bounded case; global floor stays25
$case.Run.FrameEvery = 12
$case.DiagEnv += ',BRN_TRAIL_PIXEL_DUMP=1,BRN_TRAIL_PIXEL_START=36,BRN_FRAME_DUMP_MAX=360'
$case.Checks += @(
    @{ Kind='LogMatch'; Name='native positive trail-only RGB on selected surviving quad'; Pattern='\[trail-pixels\].*matches=[1-9]\d* .*expired=0 .*roiValid=1 .*written=1/1 rgb=[1-9]\d* .*roiRgb=[1-9]\d* .*hr=00000000' }
    @{ Kind='LogMatch'; Name='valid native zero coverage after original expiry'; Pattern='\[trail-pixels\].*matches=0 .*expired=1 .*roiValid=1 .*written=1/1 .*roiRgb=0 .*hr=00000000' }
    @{ Kind='LogCount'; Name='bounded twelve native trail pairs'; Pattern='\[trail-pixels\].*written=1/1'; Min=2; Max=12 }
)
$case
