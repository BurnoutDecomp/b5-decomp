# FX-SCENARIOS (crash parity 2026-09-25) -- the traffic probe's REVERSING variant: the player car is placed WITH the
# lane's flow (heading 180, RivalOrganic's own heading) at a standstill and then BACKS UP the lane on a held brake
# (reverse), into the traffic coming up behind it. Reverse speed is low, so every meeting is a SLAM, and the held
# brake keeps the player's tail pressed into the car it met: the pin CheckIfPhysicalVehicleIsStuck tests, with the
# queue behind that car closing the other end. Everything else is FxScenariosTrafficProbe.ps1's (free roam, the
# crash sweep re-placing the car every 900 sim frames, INFO checks only).
# RESULT (kept as the negative control): fxscenarios_traffic_reverse_probe/20260925_111046 (exe db6f8cabfffe) reached
# fewer arms than the handbraked roadblock -- 1 dispatch, 10 stuck responses, no reversal, 1 give-up hand-back, no
# turn -- so the per-case drives use FxScenariosTrafficProbe.ps1.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxScenariosTrafficReverseProbe.ps1
$case = & (Join-Path $PSScriptRoot 'FxScenariosTrafficProbe.ps1')
$case.Name = 'fxscenarios_traffic_reverse_probe'
$case.Run.ThrottleScript = '0:accel,3:brake'
$case.Run.CrashSweepShots = (@(1..12) | ForEach-Object { '180:0' }) -join ','
$case
