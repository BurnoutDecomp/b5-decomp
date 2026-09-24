# FX-XLANE live witness for SweetenersEffect::Attach @0x826FD7A0 (crash parity FOLLOWUPS 12).
# The organic rival-damage case with the sound family's opt-in witness latch (BRN_HUD_SOUND_DIAG)
# armed. Attach prints one [sweeteners-attach] line per attach (capped at 400): the seed the TWO
# module-RNG draws built and the square-wave windows it handed over. The console windows are
# up (flt_82001CC0 0.0, flt_82002138 0.01) and down (flt_82F2CD50 0.025, flt_82F2CD4C 0.15).
# Run: python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxXlaneSweetenersLive.ps1 --run-name fxxlane_sweeteners
$case = & (Join-Path $PSScriptRoot 'RivalDamage.ps1')
$case.Name = 'fxxlane_sweeteners'
$case.DiagEnv += ',BRN_HUD_SOUND_DIAG=1'
$case.Checks += @(
    @{ Kind = 'Script'; Name = 'SweetenersEffect::Attach dispatched with the console square-wave windows'; Script = {
        param($ctx)
        $hits = @($ctx.LogLines | Where-Object { $_ -match '\[sweeteners-attach\]' })
        $good = @($hits | Where-Object { $_ -match 'up \(0\.000, 0\.010\) down \(0\.025, 0\.150\)' })
        @{ Pass = ($hits.Count -gt 0 -and $good.Count -eq $hits.Count);
           Detail = "$($hits.Count) attach witness line(s), $($good.Count) with up (0.000, 0.010) down (0.025, 0.150)" }
    } }
)
$case
