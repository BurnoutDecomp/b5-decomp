# FX-AINAN2 (crash parity 2026-09-24, CC-10): BrnAI::AIModule::OnRaceCarReachedCheckpoint @0x8278A658
# runs live. It was a named park until e0920bae; the body now hands the car its next checkpoint
# (AICar::OnReachedCheckpoint: route invalidated, destination section, miCurrentCheckpoint + 1,
# distance reset, block-checkpoint flag) and, for an opponent, tells the rubber band
# (RaceBalancingManager::OnOpponentReachedCheckpoint, the bl @0x8278A700).
# Same race as FxGs2Tailing.ps1 (junction 480886, -Teleport "3003.9,6.6,-1675.6,0" -StartEvent),
# driven by tests/run_rival_organic.py's pad pursuit; its first run (fxgs2_tailing/20260923_135945)
# dispatched action 113 six times (every car reaches the finish checkpoint) inside 120 s.
# The body's own [FLAG PC witness] prints the first 16 hand-offs:
#   [ai-evt] checkpoint: car <global> passed cp <n> -> next section <s> (opponent: rubber band told)
# so a pass proves the dispatched action reached the new body -- not just the dispatcher.
# This junction's races carry one checkpoint (the finish); a multi-checkpoint event would show the
# opponents re-routing between checkpoints, which is what the witness is kept for.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxAinan2CheckpointLive.ps1 --run-name fxainan2_checkpoint
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxainan2_checkpoint_live'
$case.Area = 'ai'
$case.Bug = 'AIModule::OnRaceCarReachedCheckpoint (action 113) must run its ARTIST body: re-aim the car at its next checkpoint and, for an opponent, tell the race-balancing rubber band.'
$case.Run.Teleport = '3003.9,6.6,-1675.6,0'
$case.Run.MaxSeconds = 120
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'action 113 dispatched to the AI'; Pattern = '\[ai-evt\] action 113 RACE_CAR_REACHED_CHECKPOINT'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'an opponent hand-off ran the body and told the rubber band'; Pattern = '\[ai-evt\] checkpoint: car [1-9]\d* passed cp -?\d+ -> next section \d+ \(opponent: rubber band told\)'; Expect = $true }
    @{ Kind = 'Script'; Name = 'every witnessed action 113 dispatch reached the body'; Script = {
        param($ctx)
        # The dispatcher's own witness stops after 8 per action (WitnessGameAction), the body's after
        # 16, so every printed dispatch must have a body line: witnessed >= dispatched.
        $dispatched = @($ctx.LogLines | Where-Object { $_ -match '\[ai-evt\] action 113 RACE_CAR_REACHED_CHECKPOINT' }).Count
        $witnessed  = @($ctx.LogLines | Where-Object { $_ -match '\[ai-evt\] checkpoint: car \d+ passed cp' }).Count
        @{ Pass = ($dispatched -gt 0 -and $witnessed -ge $dispatched); Detail = "$dispatched dispatches printed (cap 8), $witnessed body witnesses (cap 16)" }
    } }
)
$case
