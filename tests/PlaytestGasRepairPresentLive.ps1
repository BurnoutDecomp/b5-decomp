# Resolve the camera/flash relationship at actual presents, with no timing change.
$case = & (Join-Path $PSScriptRoot 'PlaytestGasRepairFlashLive.ps1')
$case.Name = 'playtest_gas_repair_present'
$case.Run.FrameEvery = 1
$case.DiagEnv = $case.DiagEnv.Replace('BRN_FRAME_DUMP_MAX=480', 'BRN_FRAME_DUMP_MAX=600')
$case.DiagEnv += ',BRN_REPAIR_FRAME_DIAG=1'
$case.Checks += @(
    @{ Kind='LogCount'; Name='actual final-composite camera/tint samples'; Pattern='^\[repair-frame\] present='; Min=60; Max=900 }
    @{ Kind='LogMatch'; Name='white tint reaches an allowed real post-FX pass'; Pattern='^\[repair-frame\] present=\d+ postfx=1 allowed=1 tint=1,1,1,1 ' }
)
$case
