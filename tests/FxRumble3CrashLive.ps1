# FX-RUMBLE3 (crash parity 2026-09-24, G10-D4) -- a crash's jolt reaches the pad's motor request, live.
#
# The chain, console order (every link is the ARTIST body, bar the flagged PC seats):
#   RumbleManager::Update @0x82386A98 crash latch -> PlayJolt(1010) @0x8236E7F8     "[rumble] jolt prio=1010 queued=1"
#   BrnGameModule::DoUpdate_InputPreWorld @0x823C5650 (the first leg of the update step)
#     -> GameStateModule::BridgeRumbleToInput @0x8236B570 -> RumbleManager::BridgeRumbleToInput @0x82364978
#        (the four queues posted into the "InputPreWorld" PreWorldInputBuffer, the flags published)
#                                                                                  "[rumble] bridge jolts=N ..."
#     -> InputModule::PreWorldUpdate @0x82903328 -> ProcessRumbleRequests @0x828FFE50
#        -> InputPads::PlayJoltEvent @0x828DC128 (player 0 -> port 0: the PC's standing bind, UpdatePadDevices)
#        -> InputPads::UpdatePadRumble @0x828EFC58 (the envelope walk -> the motor speeds)
#                                                                                  "[rumble] pad-request port=0 ..."
#        -> DeviceX360Pad::SetRumble @0x828E78D0 -> XInputSetState (PC motor leaf)  "[rumble] motor ..." (a pad only)
# This box has no XInput pad (XInputGetState answers 1167 on every user), so the pad-request line -- printed by
# UpdatePadRumble BEFORE its connected gate -- is the last hop the run can see; with a pad plugged in the same
# request goes on to SetRumble and the motor line.
#
# The drive is the FX-CRASHSND2 deterministic three-shot sweep (quay rocks at 44 m/s, the harbour slope,
# 53 m/s): every shot is a hard crash, so the crash-start jolt (priority 1010) fires on each.
# Witnesses (BRN_RUMBLE_DIAG, NOT IN THE X360 BINARY): the three lines above.
@{
  Name    = 'fxrumble3_crash'
  Area    = 'rumble'
  Bug     = 'A crash''s rumble request must travel RumbleManager -> DoUpdate_InputPreWorld -> BridgeRumbleToInput -> InputModule::ProcessRumbleRequests -> UpdatePadRumble: the jolt queue is drained every update step and the crash jolt drives port 0''s motor request.'
  Frames  = $false
  Run     = @{
    Drive            = $true
    MotionProbe      = $true
    MaxSeconds       = 110
    SkipIntro        = $true
    AcceptGap        = 1.0
    CrashSweep       = '1790.5,-3.2,-2393.0'
    CrashSweepShots  = '1790.5/-3.2/-2393.0/260:44,1758.5/-0.8/-2399.6/262:12,1790.5/-3.2/-2393.0/260:53'
    CrashSweepSettle = 300
  }
  DiagEnv = 'BRN_RUMBLE_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';      Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount';  Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'LogCount';  Name = 'the sweep fired (the car was placed)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount';  Name = 'the crash-start jolt was produced and queued (PlayJolt prio 1010)'
       Pattern = '^\[rumble\] jolt prio=1010 queued=1 '; Min = 1 }
    @{ Kind = 'LogCount';  Name = 'BridgeRumbleToInput drained queued jolts into the pre-world input buffer'
       Pattern = '^\[rumble\] bridge jolts=[1-9]'; Min = 1 }
    @{ Kind = 'LogCount';  Name = 'the drain keeps the queue open: no jolt refused for a full queue'
       Pattern = '^\[rumble\] jolt prio=\d+ queued=0 '; Max = 0 }
    @{ Kind = 'LogCount';  Name = 'UpdatePadRumble turned a jolt into a motor request on port 0 (player 0''s pad)'
       Pattern = '^\[rumble\] pad-request port=0 '; Min = 1 }
    @{ Kind = 'Script';    Name = 'the CRASH jolt (1010) itself reached the motor request, after its bridge'; Script = {
        param($ctx)
        $produced = -1; $bridged = -1; $requested = -1; $i = 0
        foreach ($l in $ctx.LogLines) {
          if ($produced -lt 0 -and $l -match '^\[rumble\] jolt prio=1010 queued=1 ') { $produced = $i }
          elseif ($produced -ge 0 -and $bridged -lt 0 -and $l -match '^\[rumble\] bridge jolts=[1-9]') { $bridged = $i }
          elseif ($bridged -ge 0 -and $requested -lt 0 -and $l -match '^\[rumble\] pad-request port=0 .*jolt\{prio=(1010,|-?\d+,1010 )') { $requested = $i }
          $i++
        }
        @{ Pass = ($requested -ge 0); Detail = "crash jolt line $produced, bridge line $bridged, pad request line $requested" }
      } }
  )
}
