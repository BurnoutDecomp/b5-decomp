# FX-BRIDGES live case: the crash-parity RivalDamageHinge drive (scratch/CRASHPARITY_0922/RivalDamageHinge.ps1 =
# RivalDamage + BRN_DEFORM_TRACE) with the camera-input diagnostic BRN_CAM_INPUT_DIAG, whose
# `[cam-impact]` line is printed by BrnGameModule::BridgeWorldToDirector @0x823E3AB0 each time a car's
# published VehicleInfo::mfHardestImpact is non-zero (the first 40, then every 200th). Before CC-5 that value
# was 0.0 for every car on every frame, so the gameplay / bumper / bystander impact shake never moved; the line
# proves the hardest-impact leg was DISPATCHED on real contacts (a green run that never reached it would prove
# nothing). Diagnostics only print; the drive and every RivalDamage check are unchanged.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxBridgesLive.ps1 --run-name fxbridges
$case = & (Join-Path $PSScriptRoot '..\..\scratch\CRASHPARITY_0922\RivalDamageHinge.ps1')
$case.Name = 'fxbridges_live'
$case.DiagEnv += ',BRN_CAM_INPUT_DIAG=1'
$case.Checks += @(
    @{ Kind = 'Script'; Name = 'CC-5: the player car publishes a non-zero hardest impact to the director (impact shake input)'; Script = {
        param($ctx)
        $lines = @($ctx.LogLines | Where-Object { $_ -match '\[cam-impact\] #\d+ slot \d+ \(player\) mfHardestImpact ([-+0-9.eE]+)' })
        $max = 0.0
        foreach ($l in $lines) {
            if ($l -match 'mfHardestImpact ([-+0-9.eE]+)') {
                $v = [double]::Parse($Matches[1], [cultureinfo]::InvariantCulture)
                if ($v -gt $max) { $max = $v }
            }
        }
        @{ Pass = ($lines.Count -gt 0 -and $max -gt 0.0); Detail = "$($lines.Count) player [cam-impact] lines, max mfHardestImpact $max" }
    } }
    @{ Kind = 'Script'; Name = 'CC-5: a rival car publishes a non-zero hardest impact too (bystander / crash cameras)'; Script = {
        param($ctx)
        $lines = @($ctx.LogLines | Where-Object { $_ -match '\[cam-impact\] #\d+ slot \d+ mfHardestImpact' })
        @{ Pass = $lines.Count -gt 0; Detail = "$($lines.Count) non-player [cam-impact] lines" }
    } }
)
$case
