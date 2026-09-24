# FX-EMITTER (crash parity 2026-09-24): the site witness for FxEmitterAttachLive.ps1. The junction-480886
# pursuit asserted `luEmitter < static_cast< uint32_t >( lWorldEmitters.mNumWorldEmitters() )` on ONE
# entity: TRK_UNIT146 trk_unit146_emitter entity 0, TrainStation1 at (2851.2, -6.7, -1403.3), X360 W lane
# 0x001D00AD -> type 29 (W >> 16, EmitterEffect::Attach @0x826F57F8), radius 173 (W & 0xFFFF,
# QuerySoundMap @0x826D18F0). A pre-2026-08-31 conversion stored it as 0x1D00AD00 (type 173). The pursuit
# route does not reach it on every exe (the car can wedge first), so this case places the car on the road
# 66 m from it -- the point the 20260924_153544 pursuit crossed heading +Z at 43 m/s -- and holds the
# throttle. BRN_EMITTER_DIAG=1 arms KB_DEBUG_WORLD_EMITTERS (the luEmitter assert) and the witness.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxEmitterSiteLive.ps1
$case = & (Join-Path $PSScriptRoot 'FxEmitterAttachLive.ps1')
$case.Name = 'fxemitter_site'
$case.Bug = 'The TRK_UNIT146 TrainStation1 emitter (type 29, radius 173) attaches through the armed console gate.'
$case.Run.Teleport = '2793.1,-2.9,-1435.1,0'
$case.Run.StartEvent = $false
$case.Run.MaxSeconds = 120
$case.Checks = @(
    @{ Kind = 'LogCount'; Name = 'zero asserts of any kind'; Pattern = '^\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'no luEmitter assert (Attach @0x826F5800)'; Pattern = 'luEmitter < static_cast'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the console gate refused no entity'; Pattern = '\[emitter\] REFUSED'; Max = 0 }
    @{ Kind = 'LogMatch'; Name = 'TRK_UNIT146 TrainStation1 attaches as type 29 radius 173 at (2851.2, -6.7, -1403.3)'; Pattern = '\[emitter\] attach type 29 of 38 name \S*TrainStation1\S* radius 173\.0+ pos \(2851\.2'; Expect = $true }
    @{ Kind = 'Script'; Name = 'attached world emitters reported'; Script = {
        param($ctx)
        $lines = @($ctx.LogLines | Where-Object { $_ -match '\[emitter\] attach type (\d+) of (\d+) name (\S+) radius (\d+)' })
        $seen = @($lines | ForEach-Object {
            if ($_ -match '\[emitter\] attach type (\d+) of (\d+) name \S*/([^/?]+?)(\.wav)?(\.WaveFile\S*)? radius (\d+)') { "$($Matches[1]):$($Matches[3]):r$($Matches[6])" } })
        @{ Pass = ($lines.Count -gt 0); Detail = "$($lines.Count) attach line(s) (witness capped at 48): $(($seen | Select-Object -Unique) -join ', ')" }
    } }
)
$case
