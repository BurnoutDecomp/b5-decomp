# FX-TRAFFIC2 (crash parity 2026-09-24, G32-D1) -- live witness for the detached-wheel early accept
# in PhysicalTrafficManager::ValidateTrafficContact @0x825CACB8 (0x825CAEAC `lbz 0x715 ; bne -> li r3,1`).
# The arm runs for a PHYSICAL TRAFFIC car's contact with the WORLD (B owner 0) once that car's own body has a
# detached wheel (SimpleVehiclePhysics::CalculateNewWheelPlane sets mbAnyWheelsDetatched @0x82602D48 when a
# wheel's mu8State is 2). Traffic wheels come off in DeformableObject::UpdateWheels while the traffic car is
# crashing, and the sure route is the Showtime bounce arm: ApplyCarCarImpulse @0x82625008..0x82625040 rolls
# `(seed hi % 3) == 0` into the TRAFFIC car's mbForceWheelsToDetach on every Showtime car-vs-traffic contact,
# which twists all four wheels on its next crashing UpdateWheels and sheds them on the pass after.
#
# FX-SCENARIOS (2026-09-25): the first drive (ShowtimeContacts alone) landed few Showtime bounces on traffic: none
# in scratch/bugtest/runs/fxtraffic2_wheels_detached/20260924_075232 and 20260924_230728 (their [showtime-contact]
# lines are all world=1, the WORLD bounce), 3 on two cars in 20260925_062543 ([showtime-car-contact], BRN_SHOWTIME_
# WATCH, capped at 8) -- each a 1-in-3 draw, none detached, one wheel twisted by its own deformation (obj 2 wheel 0,
# force 0, the car at rest so the 8.9408 m/s detach gate stayed shut). The drive is now FxTailsACrashPlayLive's:
# the same fresh-profile Showtime, with the boost button pulsed from 41 s (flow_run -Boost), so the car keeps
# bouncing and lands on traffic (that drive scored 6 traffic hits, 5 car contacts, in fxtailsa_crash_play/
# 20260925_080443). Two INFO checks say what the run offered the arm: the Showtime traffic hits and car contacts,
# and the traffic wheels the ladder twisted / detached.
# BRN_WHEEL_PROBE shows the detach; BRN_TRAFFIC_DIAG arms the capped [T-contact] line the arm prints each time it
# accepts a world contact for a wheel-less traffic car.
# First GREEN on this drive: fxtraffic2_wheels_detached/20260925_122916 (exe 06dbcfbcae64) -- a Showtime car contact
# with traffic (trafficMass 1150), TWIST obj 1 wheels 0..3, all four `force 1 -> DETACH`, then `[T-contact]
# wheels-detached accept slot=0 belowWheelPlane=0`; a second car (obj 5) shed all four as well.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxTraffic2WheelsDetachedLive.ps1
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'fxtraffic2_wheels_detached'
$case.Area = 'traffic'
$case.Bug = 'A traffic car with a detached wheel must keep every world contact (it rests on its body), not have its road contacts filtered as if its wheels held it up (G32-D1).'
$case.DiagEnv += ',BRN_TRAFFIC_DIAG=1,BRN_WHEEL_PROBE=1,BRN_CRASHPLAY_TRACE=1'
# FX-DIRECTOR2 2026-09-25: 50 / 179 (were 41 / 170). The boost still starts 1 s after ShowtimeContacts' gesture,
# which moved 40 -> 49 to clear the crash analyser's 401-update hold on the drive's first wall crash
# (see ShowtimeContacts.ps1).
$case.Run.Boost = '50'
$case.Run.MaxSeconds = 179
$case.Checks += @(
    @{ Kind = 'Script'; Name = 'INFO -- what the Showtime offered the arm: traffic hits and car contacts (never fails)'; Script = {
        param($ctx)
        $hits = @($ctx.LogLines | Where-Object { $_ -match '\[showtime-score\] answer traffic car ' }).Count
        $world = @($ctx.LogLines | Where-Object { $_ -match '\[showtime-contact\] world=1 ' }).Count
        $car = @($ctx.LogLines | Where-Object { $_ -match '\[showtime-car-contact\] other=' }).Count
        @{ Pass = $true; Detail = "$hits scored Showtime traffic hit(s); [showtime-contact] world=1 x$world; [showtime-car-contact] (capped at 8) x$car" }
    } }
    @{ Kind = 'Script'; Name = 'INFO -- traffic wheels the UpdateWheels ladder twisted / detached (obj != 0; never fails)'; Script = {
        param($ctx)
        $twist = @{}; $detach = @{}
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[wheel-probe\] TWIST obj (\d+) wheel (\d+)' -and $Matches[1] -ne '0') { $twist["$($Matches[1])/$($Matches[2])"] = 1 }
            if ($l -match '\[wheel-probe\] obj (\d+) wheel (\d+) state 1 .* -> DETACH' -and $Matches[1] -ne '0') { $detach["$($Matches[1])/$($Matches[2])"] = 1 }
        }
        @{ Pass = $true; Detail = "twisted obj/wheel: [$(($twist.Keys | Sort-Object) -join ', ')]; detached: [$(($detach.Keys | Sort-Object) -join ', ')]" }
    } }
    @{ Kind = 'LogMatch'; Name = 'G32-D1 the detached-wheel arm accepted a world contact'
       Pattern = '\[T-contact\] wheels-detached accept slot=\d+'; Expect = $true }
)
$case
