# FX-BRIDGES CC-8 live case, FORCED variant: FxBridgesTakedownCamLive.ps1 with the PC harness knob
# BRN_FORCE_PLAYER_TAKEN_DOWN=15 -- the console's own "Force takedown" debug action
# (TakedownManagerDebugComponent::ForceTakedownCallback @0x823597F8) with the roles swapped: 15 s into the Road
# Rage mode the first active, non-crashing rival is armed as the aggressor of a STANDARD takedown of the player
# (TakedownManager::HarnessForceTakenDown, GameStateModule_gTD_00.cpp; re-armed every 10 s, at most 3 times).
# Only the CREDIT is forced: the confirmation sweep (ProcessQueuedTakedowns), ProcessTakedownEvent, the output
# takedown queue, BridgeGameStateToDirector's taken-down leg and the director's read are all the real code.
# Three organic runs (scratch/bugtest/runs/fxbridges_cc8/20260924_134412, _140644, _141254) produced no rival credit
# on a player crash, so the organic case could not witness the leg; this one can.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxBridgesTakedownCamForcedLive.ps1 --run-name fxbridges_cc8_forced
$case = & (Join-Path $PSScriptRoot 'FxBridgesTakedownCamLive.ps1')
$case.Name = 'fxbridges_takedown_cam_forced'
$case.Bug = 'A (harness-credited) rival takedown of the player must reach the director input and be read by the director.'
$case.Run.MaxSeconds = 120
$case.DiagEnv += ',BRN_FORCE_PLAYER_TAKEN_DOWN=15'
$case.Checks = @($case.Checks | Where-Object { $_.Name -ne 'no injected takedown' })
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'CC-8 (forced): the reversed force-takedown harness armed a rival on the player';
       Pattern = '\[td\] HARNESS force-taken-down fired'; Expect = $true }
)
$case
