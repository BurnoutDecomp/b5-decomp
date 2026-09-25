# FX-FOLLOWUPS (crash parity 2026-09-25, item 3) -- live witness for the Showtime leap TARGET ASSIST past
# `[stomp] ProcessLeapedAndStompedCars stored N stompee(s)`.
#
# The chain after [stomp] (every hop verified against ARTIST in scratch/CRASHPARITY_0922/fixes/FX-FOLLOWUPS.md):
#   RaceCarEntityModule::ProcessPlayerVehicleInput's AddTargetAssist loop (0x82300128..0x82300168)
#   -> VehicleDriverInputInterface::AddTargetAssist @0x822A0398 -> Append @0x823DB640 (WorldBridgeInputToPhysicsModule)
#   -> VehicleManager::UpdateDrivers @0x82642C68 -> GetTargetAssistParams @0x823A7B40 -> msPlayerParams
#   -> RaceCarPhysics::UpdateTargetAssist @0x8261FF50 (called by UpdateAftertouch in Showtime).
# UpdateTargetAssist only SELECTS a target when the stick aim meets it: the aim is UpdateAftertouch's
# normalize(-(cameraX * yaw + cameraZ * pitch)) (0x8262F26C-F2C8), so a throttle-only drive (ShowtimeContacts,
# FxRcem4StompeesLive) has a zero aim and can never pass the 0.766 dot gate. This case adds a steer during the
# Showtime (full lock, alternating sides, because the stompees' bearing is not known in advance). The steer is
# the aftertouch yaw: it points the aim at +/- camera X and pushes the car toward that side.
#
# Harness-only, default-off: BRN_TASSIST_DIAG arms the capped [tassist] witness (RaceCarPhysics.cpp). The
# schedule is FX-DIRECTOR2's +9 s shifted ShowtimeContacts one (Showtime at DRIVING+49 s, MaxSeconds 149), so the
# first crash's hard-stop hold, once that lands, still leaves the drive time to reach the Showtime.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxFollowupsTargetAssistLive.ps1
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'fxfollowups_target_assist'
$case.Area = 'physics'
$case.Bug = 'In Showtime, a stored stompee must reach RaceCarPhysics::UpdateTargetAssist and be selected when the aftertouch aim points at it (the leap target assist past [stomp]).'
$case.Run.Showtime = '49'
$case.Run.MaxSeconds = 149
# SteerScript t=0 is the first input (DRIVING + DriveDelay 6 s), so the Showtime press is at t=43. Steer from
# the press through the flight, alternating the side every 2 s, then release.
$case.Run.SteerScript = '43:left,45:right,47:left,49:right,51:left,53:right,55:left,57:right,59:none'
$case.DiagEnv += ',BRN_TRAFFIC_DIAG=1,BRN_STOMP_DIAG=1,BRN_TASSIST_DIAG=1'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'CHAIN-STOMPEES the producer published a stompee'
       Pattern = '\[T5-stomp\] .* stompees=[1-8] '; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'CHAIN-STOMPEES c the module stored the stompees (Showtime + airborne)'
       Pattern = '\[stomp\] ProcessLeapedAndStompedCars stored [1-8] stompee'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'TARGET ASSIST the stompees reached UpdateTargetAssist'
       Pattern = '\[tassist\] call=\d+ targets=[1-8] '; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'TARGET ASSIST evaluated past the 0.5 s airtime gate with a target'
       Pattern = '\[tassist\] call=\d+ targets=[1-8] air=\S+ gate=1 '; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'TARGET ASSIST ENGAGED: a stored stompee was selected (dot > 0.766)'
       Pattern = '\[tassist\] .* gate=1 .* best=[0-7] id=\d+ '; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'TARGET ASSIST pulled toward the stompee (the > 2 m assist force fired)'
       Pattern = '\[tassist\] .* fired=1 force=\d'; Expect = $true }
)
$case
