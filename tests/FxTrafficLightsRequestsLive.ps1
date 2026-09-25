# FX-TRAFFICLIGHTS (crash parity wave 5, 2026-09-25) -- live witness of the traffic's game-action arms on a normal boot
# through the junkyard car select, and of the producer fix that feeds arm 77.
#   73 CAR_SELECT_TRANSITION_IN  (0x8274C018) CarSelectManager's transition-in start widens the traffic box:
#                                 mfTrafficSimRadius splat(395.0), 64 cars within 400 m, mbInOfflineCarSelect
#   75 CAR_SELECT_READY          (0x8274C000) the junkyard posts type 1: the traffic is NOT hidden (only type 2, online)
#   77 CAR_SELECT_EXIT           (0x8274C068) offline: KillAllTrafficInCylinder(the record's exit spawn position, 150 m,
#                                 1000 m, false) and Construct's box back (195.0, 32 cars, 62500.0). The record's position
#                                 comes from CarSelectManager::UpdateExitState (0x82398D6C), which posted 32 zero bytes
#                                 before this wave: the centre was the world origin.
# The car then drives off; if the drive crosses a kill-zone trigger, 110 / FireKillZone lines follow (not required).
# BRN_TRAFFIC_TRACK arms the [traffic-track] lines (FLAG PC, reads only; RemoveVehicle prints each removal with its
# reason tag: carselect-exit / killzone / ...).
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxTrafficLightsRequestsLive.ps1
@{
  Name    = 'fxtrafficlights_requests'
  Area    = 'traffic'
  Bug     = 'HandleExternalRequests arms 13/30/73/75/77/110/192/244 + the Picture Paradise tail were one gate; the junkyard exit (77) posted no exit position.'
  Frames  = $false
  Run     = @{
    Drive          = $true
    MotionProbe    = $true
    AcceptGap      = 1.0
    DriveDelay     = 2.0
    ThrottleScript = '0:accel,25:none'
    MaxSeconds     = 110
  }
  DiagEnv = 'BRN_TRAFFIC_TRACK=1'
  Checks  = @(
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions (0 AV)'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark';       Name = 'reached DRIVING through the car select'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch';   Name = '73: the car select widens the traffic box (395 m, 64 cars within 400 m)'
       Pattern = '\[traffic-track\] request 73 car-select transition-in start=1 simRadius=395(\.0+)? maxRender=64 cullDistSq=160000(\.0+)? inCarSelect=1'; Expect = $true }
    @{ Kind = 'LogMatch';   Name = '75: the junkyard car select (type 1) leaves the traffic visible'
       Pattern = '\[traffic-track\] request 75 car-select ready type=1 hidden=0'; Expect = $true }
    @{ Kind = 'LogMatch';   Name = '77 offline: the cylinder and Construct''s box back (195 m, 32 cars)'
       Pattern = '\[traffic-track\] request 77 car-select exit online=0 cylinder centre=\(.*\) r=150(\.0+)? h=1000(\.0+)? restoredBox=1 simRadius=195(\.0+)? maxRender=32 hidden=0'; Expect = $true }
    @{ Kind = 'Script';     Name = '77: the cylinder centre is the junkyard-exit spawn position (where the car comes out), not the origin'; Script = {
        param($ctx)
        $centre = $null; $car = $null
        foreach ($l in $ctx.LogLines) {
          if ($null -eq $centre -and $l -match '\[traffic-track\] request 77 car-select exit online=0 cylinder centre=\((?<x>[-\d.]+),(?<y>[-\d.]+),(?<z>[-\d.]+)\)') {
            $centre = @([double]$Matches.x, [double]$Matches.y, [double]$Matches.z)
            continue
          }
          if ($null -ne $centre -and $null -eq $car -and $l -match '\[motion\] n \d+ pos (?<x>[-\d.]+) (?<y>[-\d.]+) (?<z>[-\d.]+)') {
            $car = @([double]$Matches.x, [double]$Matches.y, [double]$Matches.z)
          }
        }
        if ($null -eq $centre) { return @{ Pass = $false; Detail = 'no request-77 offline line' } }
        $origin = [math]::Sqrt($centre[0] * $centre[0] + $centre[2] * $centre[2])
        if ($null -eq $car) { return @{ Pass = $false; Detail = "centre=($($centre -join ', ')) but no [motion] sample after it" } }
        $dx = $centre[0] - $car[0]; $dz = $centre[2] - $car[2]
        $ground = [math]::Sqrt($dx * $dx + $dz * $dz)
        return @{ Pass = ($origin -gt 100.0 -and $ground -lt 15.0)
                  Detail = ("centre=({0:F2}, {1:F2}, {2:F2}) {3:F1} m from the origin; the car's first [motion] position ({4:F2}, {5:F2}, {6:F2}) is {7:F2} m from it on the ground" -f $centre[0], $centre[1], $centre[2], $origin, $car[0], $car[1], $car[2], $ground) }
      } }
    @{ Kind = 'LogCount';   Name = 'no assert from the traffic module, the car-select manager or the kill-zone data'
       Pattern = '\[ASSERT \d+\].*(BrnTrafficEntityModule|BrnCarSelectManager|BrnTrafficData)'; Max = 0 }
  )
}
