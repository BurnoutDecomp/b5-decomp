# Capture the existing glass/debris draw scenario for visual verification of
# native per-batch constant publication. The shared runner owns slot isolation.
$case = & (Join-Path $PSScriptRoot 'FxCrashVfxDebrisDrawLive.ps1')
$case.Name = 'playtest_render_glass_1005'
$case.Bug = 'Glass/debris meshes draw with the transform array belonging to each batch; inspect shards and effects after the wall crash.'
$case.Frames = $true
$case.ProfileFixture = 'rival_hunt_profile.sav'
$case.Run.MaxSeconds = 100
$case.Run.SkipIntro = $true
$case.Run.AcceptGap = 1.0
$case.DiagEnv += ',BRN_BOOSTLOC_DIAG=1,BRN_DEFORMLOC_DIAG=1,BRN_SWEEP_WAIT_ROAMING=1,BRN_FRAME_DUMP_MAX=240'
$case
