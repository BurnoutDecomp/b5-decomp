# FX-EMITTER (crash parity 2026-09-24): the world-emitter attach gate, live. The dev assert
# `luEmitter < static_cast< uint32_t >( lWorldEmitters.mNumWorldEmitters() )` (EmitterEffect::Attach
# @0x826F5740) paused the race junction 480886 pursuit near (2830, -2, -1423): TRK_UNIT146's
# TrainStation1 emitter at (2851.2, -6.7, -1403.3). Same drive as FX-FLOW's FxFlowRaceHudBridge.ps1:
# -Teleport "3003.9,6.6,-1675.6,0" -StartEvent, tests/run_rival_organic.py's pad pursuit.
# Needs build/game's StaticSoundMaps at the committed porter's output (see
# scratch/CRASHPARITY_0922/fixes/FX-EMITTER.md: a pre-2026-08-31 conversion reads every emitter's
# radius as its type). BRN_EMITTER_DIAG=1 arms the witness in Attach:
#   `[emitter] attach type <t> of 38 name <gamedb path> radius <r> pos (...) doppler <d> stream <s>`
#   `[emitter] REFUSED type <t> >= mNumWorldEmitters 38 ...`   (the console gate refusing an entity)
# and stands in for the console's debug switches: KB_DEBUG_WORLD_EMITTERS ("Emitters"/"Debug" -- the
# luEmitter assert is ARMED in this case, so 0 asserts means the data is right, not that the assert is
# off) and KB_SPEW_STATIC_MAP_INFO (the console's "[Static map] 1: Load unit / 2: Aqcuire unit /
# 3: Resource for unit / 4: Unload unit" spew -- the measurement of whether the world streamer ever
# double-posts a zone load, which SoundWorldScene::HandleWorldZoneLoad's PC-only early-out hides).
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxEmitterAttachLive.ps1 --run-name fxemitter
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxemitter_attach'
$case.Area = 'sound'
$case.Bug = 'World emitters must attach through the console gate (type < mNumWorldEmitters) without the luEmitter assert.'
$case.Run.Teleport = '3003.9,6.6,-1675.6,0'
$case.Run.MaxSeconds = 240
$case.DiagEnv += ',BRN_EMITTER_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'zero asserts of any kind'; Pattern = '^\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'no luEmitter assert (Attach @0x826F5800)'; Pattern = 'luEmitter < static_cast'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'world emitters attach through the gate (witness armed)'; Pattern = '\[emitter\] attach type \d+ of 38 name gamedb://'; Expect = $true }
    @{ Kind = 'LogCount'; Name = 'the console gate refused no entity'; Pattern = '\[emitter\] REFUSED'; Max = 0 }
    @{ Kind = 'LogMatch'; Name = 'the console static-map spew is armed (zones load)'; Pattern = '\[Static map\] 1: Load unit\t\d+'; Expect = $true }
    @{ Kind = 'Script'; Name = 'no zone load is double-posted (a second "1: Load unit" before its "4: Unload unit")'; Script = {
        param($ctx)
        $loaded = @{}
        $dupes = @()
        $loads = 0; $unloads = 0; $stalls = 0
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[Static map\] 1: Load unit\t(\d+)') {
                $loads++
                if ($loaded.ContainsKey($Matches[1])) { $dupes += $Matches[1] } else { $loaded[$Matches[1]] = $true }
            } elseif ($line -match '\[Static map\] 4: Unload unit\t(\d+)') {
                $unloads++
                $loaded.Remove($Matches[1])
            } elseif ($line -match '\[Static map\] Stalled\.') { $stalls++ }
        }
        @{ Pass = ($dupes.Count -eq 0 -and $loads -gt 0); Detail = "$loads load(s), $unloads unload(s), $($dupes.Count) double-posted load(s) [$(($dupes | Select-Object -Unique) -join ', ')], $stalls 'Stalled.' line(s)" }
    } }
    @{ Kind = 'Script'; Name = 'attached world emitters reported'; Script = {
        param($ctx)
        $lines = @($ctx.LogLines | Where-Object { $_ -match '\[emitter\] attach type (\d+) of (\d+) name (\S+) radius (\d+)' })
        $seen = @($lines | ForEach-Object {
            if ($_ -match '\[emitter\] attach type (\d+) of (\d+) name \S*/([^/?]+?)(\.wav)?(\.WaveFile\S*)? radius (\d+)') { "$($Matches[1]):$($Matches[3]):r$($Matches[6])" } })
        @{ Pass = ($lines.Count -gt 0); Detail = "$($lines.Count) attach line(s) (witness capped at 48): $(($seen | Select-Object -Unique) -join ', ')" }
    } }
)
$case
