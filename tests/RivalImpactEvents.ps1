# Live regression for WorldModule::BridgePhysicsToOutput @0x827AEB18 legs 4-6 (2026-09-22).
# Run: python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/RivalImpactEvents.ps1 --run-name rival_impact_events
#
# VehicleManager::HandleRaceCarRaceCarContact posts world event 31 (VEHICLE_IMPACT) for every
# classified race-car impact onto the PHYSICS VehicleOutputInterface game-event queue (+0x65F0).
# Only leg 5 of BridgePhysicsToOutput (0x827AEBD0..0x827AEBE4, UpdateOutputBuffer::
# AppendGameEventQueue) carries it into the WORLD game-event queue that GameStateModule reads
# (SendVehicleImpactMessages: aggressor boost award action 53 / victim action 54) and that
# BridgeWorldImpactInformationToGui turns into GUI 365 (TRADING PAINT / SLAM / SHUNT hint).
# Before the fix legs 4-6 were parked: every organic run classified impacts ([td-contact] verdict
# impact=slam/trading-paint/...) and NONE of them reached [impact] (BRN_BOOST_TICKER_DIAG).
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'rival_impact_events'
$case.Bug = 'Classified rival impacts must reach the game state (event 31 -> boost award / impact messages).'
$case.DiagEnv += ',BRN_BOOST_TICKER_DIAG=1'
$case.Checks += @(
    @{ Kind = 'Script'; Name = 'every run with a classified race-car impact delivers event 31 to the game state'; Script = {
        param($ctx)
        $classified = @($ctx.LogLines | Where-Object {
            $_ -match '\[td-contact\] verdict \d+ vs \d+ impact=(?<name>[a-z-]+)\((?<n>\d+)\)' -and [int]$Matches['n'] -gt 0
        })
        $delivered = @($ctx.LogLines | Where-Object { $_ -match '^\[impact\] type \d+ aggressor' })
        if ($classified.Count -eq 0) {
            return @{ Pass = $false; Detail = 'no classified impact in this run -- the check cannot be evaluated (re-run)' }
        }
        @{ Pass = ($delivered.Count -gt 0); Detail = "$($classified.Count) classified impact verdict line(s); $($delivered.Count) [impact] event-31 deliveries" }
    } }
)
$case
