# FX-TAILS-B item 1 (crash parity 2026-09-24) -- the discarded-contact leg of the collision sound, live.
#
# ARTIST CollisionStateManager::UpdateResolver runs, between the traffic contacts and the glass leg
# (0x826F93C4..0x826F9408), ImportContactSpies<EventQueue<DiscardedContact,20>> @0x826DD1C0 on
# ContactSpyData's discarded queue (the inlined ContactSpyInterface::GetDiscardedContacts, h:219), one
# InputCollision (sub_826BDAE8) per record. Before the fix the PC had no accessor, no builder, no leg.
#
# CONSOLE FACT (the reason this case cannot hear a discarded contact): the queue is EMPTY on every frame
# on the console too. Its only producer is PhysicsModule::BridgeSimulationToOutput draining
# VehicleManager::mDiscardedContacts (0x825B055C..0x825B05EC), and nothing in the ARTIST image appends to
# that queue -- a raw-image sweep of every D-form word on its seats finds only Construct's bind
# (0x8263BFA0..0x8263C060), two Clears (PrepareData 0x82633710, DoCrashPrediction 0x82646198) and that
# drain; the DWARF instantiates no append on it. So the witness is "dispatched every frame, n = 0",
# which is exactly what the console does. A non-zero n would mean somebody invented a producer.
#
# The drive is the FX-CRASHSND2 deterministic three-shot sweep (quay rocks at 44 m/s, the harbour slope,
# 53 m/s), so the race-car / traffic / deformation legs around it run with real contacts in the same frames.
#
# The witness (BRN_COLLISION_AUDIO_DIAG, NOT IN THE X360 BINARY): the first three frames, every 1800th,
# and any frame whose queue is not empty:
#   [collision-audio] discarded leg dispatched frame=<k> n=<queue length> added=<inputs> total_records=<sum>
@{
  Name    = 'fxtailsb_discarded'
  Area    = 'sound'
  Bug     = 'UpdateResolver must run the discarded-contact leg between the traffic contacts and the glass leg, as the console does (over a queue the console never fills).'
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
  DiagEnv = 'BRN_COLLISION_AUDIO_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'the sweep fired (the car was placed)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the discarded leg was dispatched (first frames + periodic lines)'
       Pattern = '^\[collision-audio\] discarded leg dispatched frame=\d+ n=-?\d+ '; Min = 4 }
    @{ Kind = 'Script';     Name = 'the discarded queue carried nothing, on every frame (the console never fills it)'; Script = {
        param($ctx)
        $lines = 0; $nonEmpty = 0; $lastFrame = 0; $lastTotal = -1
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[collision-audio\] discarded leg dispatched frame=(\d+) n=(-?\d+) added=(\d+) total_records=(\d+)') {
            $lines++
            if ([int]$Matches[2] -ne 0 -or [int]$Matches[3] -ne 0) { $nonEmpty++ }
            $lastFrame = [int]$Matches[1]; $lastTotal = [int]$Matches[4]
          }
        }
        return @{ Pass = ($lines -gt 0 -and $nonEmpty -eq 0 -and $lastTotal -eq 0)
                  Detail = ('{0} witness lines, {1} non-empty, last frame {2}, records over the run {3}' -f $lines, $nonEmpty, $lastFrame, $lastTotal) }
      } }
    @{ Kind = 'LogCount';   Name = 'the dispatches span the run (a periodic line at frame 1800 or later)'
       Pattern = '^\[collision-audio\] discarded leg dispatched frame=(1800|3600|5400|7200|9000|10800) '; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the contact legs around it resolved real contacts in the run'
       Pattern = '^\[collision-audio\] resolve inputs=[1-9]'; Min = 1 }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
