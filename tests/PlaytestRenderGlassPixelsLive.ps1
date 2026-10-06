# Isolate pixels written by the ORIGINAL glass-array4 draw through native readback.
# The observer does not change any pass, material, draw or surface binding.
$case = & (Join-Path $PSScriptRoot 'PlaytestRenderGlassLive.ps1')
$case.Name = 'playtest_render_glass_pixels_1005'
$case.Run.MaxSeconds = 75
$case.Run.FrameEvery = 6
$case.DiagEnv += ',BRN_GLASS_PIXEL_DUMP=1,BRN_POSTFX_SOURCE_DUMP=1,BRN_FRAME_DUMP_MAX=1000'
$case.Checks += @(
    @{ Kind='LogMatch'; Name='actual glass-only RGB coverage and native image pair'; Pattern='\[glass-pixels\].*accepted=[1-9]\d* .*written=1/1 rgb=[1-9]\d* .*hr=00000000' }
)
$case
