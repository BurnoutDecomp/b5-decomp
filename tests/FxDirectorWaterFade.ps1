# FX-DIRECTOR live case (crash parity 2026-09-24, conductor item C1): the DROWNING FADE on a player crash into water.
# Arbitrator::Update @0x8226ADA0, NORMAL state, @0x8226AF90..0x8226B008: when the published PlayerCrashInfo says the
# player hit water (mpPlayerCrashInfo->mbHitWater, +0x27) the frame camera loses its "follow" bit, requests the
# "BlackFade_Water" start hook at blend 1.0 (flt_82001C98) with the stop hook and post-FX id cleared, and the shared
# external gameplay camera is re-armed to snap back to the car (ForcePrimaryGameplayBehaviourToFinish); otherwise
# EnsureEffectIsStopped(camera, effects, "BlackFade_Water"). The PC had both arms gated off until 2026-09-24.
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxDirectorWaterFade.ps1
# The drive is FxWaterSndLive.ps1's harbour crash sweep (its checks are kept: the crash really went into the water).
# Witnesses:
#   BRN_CRASHCAM_DIAG         `[crash-info] player slot 0 ... hitWater 1 ...` -- BridgeWorldToDirector publishing the flag
#                              (FX-BRIDGES CC-6), and the arbitrator's state edges.
#   BRN_DIRECTOR_ACTION_DIAG  `[water-fade] mbHitWater -> 1 start-hook 1 'BlackFade_Water' blend 1 ...` -- the arbitrator
#                              branch on each edge of the flag, with the published camera's hook request after it.
$case = & (Join-Path $PSScriptRoot 'FxWaterSndLive.ps1')
$case.Name = 'fxdirector_water_fade'
$case.Bug = 'A player crash into water must start the BlackFade_Water camera effect (and snap the gameplay camera back to the car) while the published crash info says the car hit water, and stop it afterwards.'
$case.DiagEnv += ',BRN_CRASHCAM_DIAG=1,BRN_DIRECTOR_ACTION_DIAG=1'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'the bridge published mbHitWater 1 for the player (PlayerCrashInfo +0x27)';
       Pattern = '\[crash-info\] player slot \d+ .*hitWater 1'; Expect = $true }
    @{ Kind = 'Script'; Name = 'the arbitrator started BlackFade_Water on the water edge (blend 1.0) and let it go afterwards'; Script = {
        param($ctx)
        $on = @($ctx.LogLines | Where-Object { $_ -match "\[water-fade\] mbHitWater -> 1 start-hook 1 'BlackFade_Water' blend 1(\.0+)? stop-hook 0" })
        $anyOn = @($ctx.LogLines | Where-Object { $_ -match '\[water-fade\] mbHitWater -> 1' })
        $off = @($ctx.LogLines | Where-Object { $_ -match '\[water-fade\] mbHitWater -> 0' })
        @{ Pass = ($on.Count -gt 0 -and $on.Count -eq $anyOn.Count);
           Detail = ("{0} rising edge(s), {1} with the console's hook request; {2} falling edge(s); first: {3}" -f
                     $anyOn.Count, $on.Count, $off.Count, ($(if ($anyOn.Count) { $anyOn[0].Trim() } else { 'none' }))) }
    } }
)
$case
