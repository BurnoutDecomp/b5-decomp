# FX-FPUMAX live case: the crash-parity RivalDamageHinge drive (scratch/CRASHPARITY_0922/RivalDamageHinge.ps1 =
# RivalDamage + BRN_DEFORM_TRACE) with one more diagnostic, BRN_CAM_INPUT_DIAG, whose first-frame
# `[cam-look] GameplayExternal` line proves BehaviourGameplayExternal::Update -- the body carrying the re-spelt
# VecFloat / fsel-order sites and the header's fsel Clamps -- was dispatched in the run (a green run that never
# reached the changed code would prove nothing). Diagnostics only print; the drive and every check are unchanged.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxFpumaxLive.ps1 --run-name fxfpumax
$case = & (Join-Path $PSScriptRoot '..\..\scratch\CRASHPARITY_0922\RivalDamageHinge.ps1')
$case.Name = 'fxfpumax_live'
$case.DiagEnv += ',BRN_CAM_INPUT_DIAG=1'
$case.Checks += @(
    @{ Kind = 'Script'; Name = 'BehaviourGameplayExternal::Update was dispatched (the chase camera ran the changed body)'; Script = {
        param($ctx)
        $lines = @($ctx.LogLines | Where-Object { $_ -match '\[cam-look\] GameplayExternal' })
        @{ Pass = $lines.Count -gt 0; Detail = "$($lines.Count) [cam-look] GameplayExternal lines" }
    } }
)
$case
