# FX-RUMBLE3 (crash parity 2026-09-24, G10-D6) -- the road-surface rumble, live.
#
# RumbleManager::Update @0x82386A98 now opens with UpdateSurfaceRumble @0x82378AE0 (0x82386AB8): each grounded
# wheel of the player's car registers its surface when that surface's rumblesurface (surface layout +0x28) has a
# priority >= 0; a newly touched surface starts a rumble (PlayRumble), a live one is refreshed every step with a
# speed-scaled volume (ChangeRumbleVolume), a surface left behind is stopped (StopRumble). The requests ride the
# G10-D4 chain: BridgeRumbleToInput posts them into the pre-world buffer, ProcessRumbleRequests plays them on
# port 0's pad, UpdatePadRumble turns the live rumble into a motor request.
#
# The drive: the car is put at (2807.6, -3.0, -1474.7) heading 26 deg and the throttle held, so it accelerates
# from rest along the striped stretch north of the junkyard where the wheels alternate surfaces 4 / 16 / 2 (the
# SKIDBRAKE/run2 skid probe logged SURFCHG surf=4 / surf=16 there, 2814,-3.6,-1461 .. 2836,-3.3,-1414). The live
# rumble table (the "[rumble] surface table" witness, run fxrumble3_surface/20260924_162743) gives surface 4
# priority 127 over 15..30 mph and surface 16 priority 128 over 0..20 mph; surfaces 1 / 2 / 10 are -1 (no rumble).
# (The first two runs used the crash sweep: the wheels only met surfaces 2 / 1 / 10, and a Director exception in
# MomentBystanderSeesAction -> BehaviourHelper::Prepare -- not this lane's code -- ended both.)
# Witnesses (BRN_RUMBLE_DIAG, NOT IN THE X360 BINARY):
#   [rumble] surface play id=<surface> prio=<p> vol=<v> ...   PlayRumble (budgeted)
#   [rumble] surface volume id=<surface> vol=<v>              ChangeRumbleVolume (budgeted)
#   [rumble] surface stop id=<surface>                        StopRumble (budgeted)
#   [rumble] bridge jolts=.. stops=.. plays=.. volumes=..     the hand-over to the input module
#   [rumble] pad-request port=0 ... rumble{prio=<a>,<b>} ...  the pad's motor request (before the connected gate)
@{
  Name    = 'fxrumble3_surface'
  Area    = 'rumble'
  Bug     = 'Rolling over a road surface that has a rumble must request it: UpdateSurfaceRumble runs first in RumbleManager::Update, the play / volume / stop requests reach the input buffer, and the surface rumble drives port 0''s motor request.'
  Frames  = $false
  Run     = @{
    Drive       = $true
    MotionProbe = $true
    MaxSeconds  = 70
    SkipIntro   = $true
    AcceptGap   = 1.0
    Teleport    = '2807.6,-3.0,-1474.7,26'
  }
  DiagEnv = 'BRN_RUMBLE_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'NewAsserts'; Name = 'no new assertions (no "Surface list appears to be corrupt", no full-queue AddEvent)' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'the surface table was read (20 surfaces, rumble refs resolved)'
       Pattern = '^\[rumble\] surface table id=4 prio=127 '; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'a grounded wheel reached a rumbling surface (4 or 16)'
       Pattern = '^\[rumble\] surface wheel=\d id=(4|16) prio=12[78] '; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'UpdateSurfaceRumble started a surface rumble (PlayRumble)'
       Pattern = '^\[rumble\] surface play id=\d+ prio=-?\d+ vol='; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'a live surface rumble was refreshed the next step (ChangeRumbleVolume)'
       Pattern = '^\[rumble\] surface volume id=\d+ vol='; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'BridgeRumbleToInput handed a surface play to the input module'
       Pattern = '^\[rumble\] bridge jolts=\d+ stops=\d+ plays=[1-9]'; Min = 1 }
    @{ Kind = 'Script';     Name = 'a surface rumble with a positive priority reached port 0''s motor request'; Script = {
        param($ctx)
        $played = @{}; $requests = 0; $firstPlay = -1; $i = 0
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[rumble\] surface play id=(\d+) prio=(\d+) ') {
            if ([int]$Matches[2] -gt 0) { $played[[int]$Matches[2]] = $true; if ($firstPlay -lt 0) { $firstPlay = $i } }
          }
          elseif ($firstPlay -ge 0 -and $l -match '^\[rumble\] pad-request port=0 .* rumble\{prio=(-?\d+),(-?\d+)\}') {
            if ($played.ContainsKey([int]$Matches[1]) -or $played.ContainsKey([int]$Matches[2])) { $requests++ }
          }
          $i++
        }
        @{ Pass = ($requests -gt 0); Detail = ("positive surface priorities played: {0}; pad requests carrying one: {1}" -f (($played.Keys | Sort-Object) -join ','), $requests) }
      } }
  )
}
