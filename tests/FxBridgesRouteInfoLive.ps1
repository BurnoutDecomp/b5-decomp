# FX-BRIDGES CC-11 live case: an event with more than one checkpoint learns its checkpoint-to-checkpoint route lengths.
# ModeManager::SetupGameMode arms the pump for a mode with more than one landmark; UpdateCheckpointDistanceRequests
# @0x823279B8 asks for one leg at a time through GameStateModule::SendRouteRequestAction @0x82381DC8 (action 50 ->
# the AI module's route planner); the answer comes back through BridgeWorldToGameState @0x823E5368 leg 10 as game
# event 174, and ProcessGameEvents' case-174 arm hands it to ModeManager::HandleCheckpointDistanceResponse
# @0x8231E6C8 -> ScoringSystem::SetCheckpointDistances per leg, ProcessFinishDistances after the last one, and the
# pump stops. Before CC-11 the request was never sent and nothing on the answer side existed.
#
# NOTE: THE PUMP ARMS ONLY FOR AN EVENT WITH MORE THAN ONE CHECKPOINT (SetupGameMode, `cmplwi r10, 1` @0x8234B788), and
# every offline event the harness has started logs `liCheckPointCount: 1` (30/30 starts, 2026-09-24) -- the
# multi-landmark events are the online races (ModeManager::SetOnlineLandmarks). The first check says which case a
# run is in; on a one-checkpoint event the route checks cannot be exercised, on the console or here.
# The drive is RivalOrganic's pad pursuit started at the offline-race junction 480886 (the FxGsRaceHudCrashes spot),
# with BRN_ROUTE_INFO_DIAG=1 (the three [route-info] witnesses) -- the AI module's own `[ai-evt] action 50` witness
# is always on.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxBridgesRouteInfoLive.ps1 --run-name fxbridges_cc11
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxbridges_route_info'
$case.Area = 'mode'
$case.Bug = 'An event with more than one checkpoint must ask for and receive every checkpoint leg length, then stop asking.'
$case.Run.Teleport = '3003.9,6.6,-1675.6,0'
$case.Run.MaxSeconds = 90
$case.DiagEnv += ',BRN_ROUTE_INFO_DIAG=1'
$keep = @('no new assertions', 'no exceptions', 'reached driving')
$case.Checks = @($case.Checks | Where-Object { $keep -contains $_.Name })
$case.Checks += @(
    @{ Kind = 'Script'; Name = 'CC-11: the started event has more than one checkpoint (SetupGameMode arms the pump only then, cmplwi r10, 1 @0x8234B788)'; Script = {
        param($ctx)
        $counts = @($ctx.LogLines | ForEach-Object { if ($_ -match 'liCheckPointCount: (\d+)') { [int]$Matches[1] } } | Select-Object -Unique)
        $multi = @($counts | Where-Object { $_ -gt 1 })
        @{ Pass = $multi.Count -gt 0
           Detail = "checkpoint count(s) of the started event: $($counts -join ', ') -- with 1 the pump is not armed (console and PC alike), so the route checks below cannot be exercised" }
    } }
    @{ Kind = 'LogMatch'; Name = 'CC-11: the mode manager asks for a leg (SendRouteRequestAction as E_OWNER_MODE_MANAGER)';
       Pattern = '\[route-info\] SendRouteRequestAction owner 2 event \d+'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'CC-11: the question reaches the AI route planner (action 50, owner 2)';
       Pattern = '\[ai-evt\] action 50 REQUEST_ROUTE_INFO owner 2'; Expect = $true }
    @{ Kind = 'Script'; Name = 'CC-11: leg 10 forwards routed answers as event 174'; Script = {
        param($ctx)
        $answers = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[route-info\] BridgeWorldToGameState leg 10: event 174 leg (\d+) nodes (\d+) distance ([-+0-9.eE]+)') {
                "leg $($Matches[1]) nodes $($Matches[2]) distance $($Matches[3])"
            }
        })
        $routed = @($answers | Where-Object { $_ -match 'nodes ([1-9]\d*) distance ([0-9.eE+]+)$' -and [double]::Parse($Matches[2], [cultureinfo]::InvariantCulture) -gt 0 })
        @{ Pass = $routed.Count -gt 0; Detail = "$($answers.Count) answer(s), $($routed.Count) routed: $((@($answers | Select-Object -Unique)) -join '; ')" }
    } }
    @{ Kind = 'Script'; Name = 'CC-11: every leg is taken once, in order, then ProcessFinishDistances stops the pump'; Script = {
        param($ctx)
        $lines = $ctx.LogLines
        $legs = @(); $stop = -1; $lastAsk = -1
        for ($i = 0; $i -lt $lines.Count; $i++) {
            $l = $lines[$i]
            if ($l -match '\[route-info\] HandleCheckpointDistanceResponse leg (\d+) distance ([-+0-9.eE]+) m; next (\d+) of (\d+)') {
                $legs += "$($Matches[1])=$($Matches[2])m"
                if ($l -match 'pump stopped' -and $stop -lt 0) { $stop = $i }
            }
            elseif ($l -match '\[route-info\] SendRouteRequestAction owner 2') { $lastAsk = $i }
        }
        $ordered = $true
        for ($k = 0; $k -lt $legs.Count; $k++) { if (-not ($legs[$k] -match "^$k=")) { $ordered = $false } }
        @{ Pass = ($legs.Count -gt 0 -and $ordered -and $stop -ge 0 -and $lastAsk -lt $stop)
           Detail = "legs $($legs -join ', '); pump stopped at log line $stop; last owner-2 ask at line $lastAsk" }
    } }
)
$case
