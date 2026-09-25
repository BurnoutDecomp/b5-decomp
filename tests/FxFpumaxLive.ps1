# FX-FPUMAX live case: the crash-parity RivalDamageHinge drive (scratch/CRASHPARITY_0922/RivalDamageHinge.ps1 =
# RivalDamage + BRN_DEFORM_TRACE) with one more diagnostic, BRN_CAM_INPUT_DIAG, whose first-frame
# `[cam-look] GameplayExternal` line proves BehaviourGameplayExternal::Update -- the body carrying the re-spelt
# VecFloat / fsel-order sites and the header's fsel Clamps -- was dispatched in the run (a green run that never
# reached the changed code would prove nothing). Diagnostics only print; the drive and every check are unchanged.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxFpumaxLive.ps1 --ai-pad pursuit --no-frames --run-name fxfpumax
# The seat is the game's own AI on the pad (--ai-pad pursuit, FX-AIPAD 2026-09-25): fxfpumax_aipad/20260925_110032 PASS
# 14/14 with 2 player-credited takedowns; the bearing-steering pad pursuit's run 20260924_222528 had none (LIVE_FINAL:
# DRIVER-LIMITED). Without --ai-pad the runner still drives the old way.
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
