# Native data/state observation over the same original four-tag boost/event path.
$case = & (Join-Path $PSScriptRoot 'PlaytestRenderBoostLinesLive.ps1')
$case.Name = 'playtest_render_band_consumer_1005'
$case.Run.MaxSeconds = 90
$case.DiagEnv += ',BRN_LION_DRAW_DIAG=DEBUGTEXTURE'
$case.DiagEnv += ',BRN_SIMPLEFX_DIAG=1'
$case.Checks += @(
    @{ Kind='LogMatch'; Name='actual selected material/resource/native consumer record'; Pattern='\[liondraw\].*material=DEBUGTEXTURE.*map=41 hash=EAF385D3 resource=00000000E7AD6945' }
    @{ Kind='LogMatch'; Name='real spark dispatch exercised'; Pattern='\[spark\] draw array \d+: texture=.*verts=[1-9]' }
    @{ Kind='LogMatch'; Name='real simple particle dispatch exercised'; Pattern='\[simplefx\] draw type \d+: texture=.*verts=[1-9]' }
)
$case
