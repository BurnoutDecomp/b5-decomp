# FX-TRAFFIC2 (crash parity 2026-09-24, G32-D1) -- live witness for the detached-wheel early accept
# in PhysicalTrafficManager::ValidateTrafficContact @0x825CACB8 (0x825CAEAC `lbz 0x715 ; bne -> li r3,1`).
# Traffic wheels only come off on the Showtime bounce arm (ApplyCarCarImpulse -> mbForceWheelsToDetach,
# a 1-in-3 roll), so ShowtimeContacts drives into Showtime. BRN_WHEEL_PROBE shows the detach;
# BRN_TRAFFIC_DIAG arms the capped [T-contact] line the new arm prints each time it accepts a world
# contact for a wheel-less car.
#
# ⚠️ NOT YET WITNESSED. On the ShowtimeContacts drive the car's Showtime bounces only ever meet the
# world (every [showtime-contact] line is world=1), so no traffic wheel detaches and the last check
# stays red: scratch/bugtest/runs/fxtraffic2_wheels_detached/20260924_075232, and the same absence in
# fxtraffic2_stompees/20260924_093330. The arm itself is pinned by run_fxtraffic2_wheels_detached.py.
# Kept for a scenario that lands a Showtime bounce on traffic; do not add it to a green sweep as is.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxTraffic2WheelsDetachedLive.ps1
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'fxtraffic2_wheels_detached'
$case.Area = 'traffic'
$case.Bug = 'A traffic car with a detached wheel must keep every world contact (it rests on its body), not have its road contacts filtered as if its wheels held it up (G32-D1).'
$case.DiagEnv += ',BRN_TRAFFIC_DIAG=1,BRN_WHEEL_PROBE=1'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'G32-D1 the detached-wheel arm accepted a world contact'
       Pattern = '\[T-contact\] wheels-detached accept slot=\d+'; Expect = $true }
)
$case
