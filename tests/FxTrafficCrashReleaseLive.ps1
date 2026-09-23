# FX-TRAFFIC (crash parity 2026-09-23, G58-X2 / G58-X3 / G58-X4) -- live witness.
# StopVehicleBeingPhysical, KillParam and StaticVehicles_KillParam now call
# EnsureVehicleRemovedFromCrashModule, which queues the car on maRecentlyRemovedVehicles; the next
# PostPhysicsUpdate drains that into the crash module's RemoveCrashedTrafficEvent queue, logged as
# "[traffic-crash] removed ... reason=explicit" (BRN_CRASH_ACTION_DIAG). Offline, nothing else
# feeds that queue (RemoveVehicle's divergent STANDARD / STATIC arms do not call Ensure), so the
# pre-fix exe logs ZERO such lines -- measured: fxcrashmod_tick/20260923_145459 and
# fxcrashmod_vehicle_input/20260923_150738 (4 and 9 traffic crashes added, 0 explicit removals).
$case = & (Join-Path $PSScriptRoot 'TrafficCrashLifecycle.ps1')
$case.Name = 'fxtraffic_crash_release'
$case.Bug = 'A crashed traffic car that is demoted or killed must be released from the crash module (EnsureVehicleRemovedFromCrashModule from StopVehicleBeingPhysical / KillParam / StaticVehicles_KillParam).'
$case.Checks += @(
    @{ Kind = 'LogCount'; Name = 'demoted/killed traffic is released through the removed-vehicle queue'
       Pattern = '\[traffic-crash\] removed vehicle=\d+ owner=-?\d+ reason=explicit'; Min = 1 }
)
$case
