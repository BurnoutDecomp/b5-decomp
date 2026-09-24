# FX-WATERSND (crash parity 2026-09-24) -- a PLAYER CRASH INTO WATER stopped the game.
#
# FxEffect::UpdateParams @0x826BC338 posts sound message 4 / FxType 8 (E_CRASH_IN_WATER) on the
# rising edge of HasCrashedIntoWater(player) (bl 0x823A7BC8 @0x826BC420), whose latch the reset
# watchdog sets when the above-ground test finds a WATER surface under the car (0x822CED44/64).
# FxEffect::Notify @0x826F7248 case 8 used to hand VoiceWrapper::Create a NULL bank:
#   [ASSERT] mCreateParams.mpContent (CgsVoiceWrapper.cpp:44), then EXCEPTION_ACCESS_VIOLATION
#   reading 0x8 (scratch/bugtest/runs/fxxlane_crashmod/20260924_074721, BrnGame.log:209335).
# The console's case 8 takes the COLLISION state manager's own splice bank --
# *(module+0x2968)+0x8228 == GetEnvironment().GetStateManager(5)->GetSplicerBank(0).
#
# WHERE THE WATER IS. The evidence run's player car ran west along the harbour road at
# ~48.5 m/s, struck the quay-side rocks at x ~1767, tipped over the edge while crashing and
# registered the water at (1747.3, -9.85, -2399.9). Measured on this build:
#   20260924_090435  straight replays (48.5 / 55 m/s) stop on / deflect off the rocks -- no water
#   20260924_091400  a seat on the rock slope (1758.5, -7.4) drives into the water, but its entry
#                    fell inside a burst of E_RESET_ON_TRACK stings holding all four FX voices,
#                    so Notify's free-slot scan dropped it (console-faithful early return)
#   20260924_092220  PASS: the same slope seat as the FIRST shot -> `[fx-sound-msg] type=8 slot=3
#                    -> PLAY sample=337 vol=8.000 bank=1`; the watchdog's drowned arm
#                    ([wrecklatch] water@0x822CED44) also fired for 260:44 and 260:53 below
# So: shot 0 is the slope entry (first, while the FX voices are free); shots 1-2 are the two
# evidence-approach variants that ended in the harbour. -CrashSweep seats the car through the
# console's own RequestPlaceOnTrack, frame-counted; -Drive holds the throttle.
#
# PASS = a water entry reached the fixed arm with a bank (the [fx-sound-msg] witness,
# BRN_HUD_SOUND_DIAG, prints `bank=1`), 0 asserts, 0 exceptions.
@{
  Name    = 'fxwatersnd_live'
  Area    = 'sound'
  Bug     = 'A player crash into water must play the crash-in-water sting on the collision splice bank, not assert and fault on a NULL bank.'
  Frames  = $false
  Run     = @{
    Drive            = $true
    MotionProbe      = $true
    MaxSeconds       = 110
    SkipIntro        = $true
    AcceptGap        = 1.0
    CrashSweep       = '1758.5,-0.8,-2399.6'
    CrashSweepShots  = '1758.5/-0.8/-2399.6/262:12,1790.5/-3.2/-2393.0/260:44,1790.5/-3.2/-2393.0/260:53'
    CrashSweepSettle = 300
  }
  DiagEnv = 'BRN_HUD_SOUND_DIAG=1,BRN_WRECK_LATCH_DIAG=1,BRN_CRASH_EXIT_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'the sweep fired (the car was placed at the harbour)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the player crash into water reached FxEffect::Notify (type 8)'
       Pattern = '\[fx-sound-msg\] type=8 '; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the type-8 sting plays on a bank (collision splice bank)'
       Pattern = '\[fx-sound-msg\] type=8 slot=\d+ -> PLAY sample=-?\d+ vol=[-\d.]+ bank=1'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'no type-8 message with a NULL bank'
       Pattern = '\[fx-sound-msg\] type=8 .*bank=0'; Max = 0 }
    @{ Kind = 'Script';     Name = 'which shot put the player in the water (attribution)'; Script = {
        param($ctx)
        $shot = -1; $pos = 'n/a'; $hits = @(); $wrecks = @()
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[sweep\] shot (?<k>\d+)/') { $shot = [int]$Matches.k; continue }
          if ($l -match '^\[motion\] n \d+ pos (?<x>[-\d.]+) (?<y>[-\d.]+) (?<z>[-\d.]+)') {
            $pos = ('({0}, {1}, {2})' -f $Matches.x, $Matches.y, $Matches.z); continue }
          if ($l -match '^\[wrecklatch\] .*water') { $wrecks += ('shot {0} near {1}' -f $shot, $pos) }
          if ($l -match '^\[fx-sound-msg\] type=8 ') { $hits += ('shot {0} near {1}: {2}' -f $shot, $pos, $l.Trim()) }
        }
        $w = if ($wrecks.Count) { ' || water-arm wrecks: ' + ($wrecks -join ' | ') } else { ' || no water-arm wreck' }
        if ($hits.Count -eq 0) { return @{ Pass = $false; Detail = 'no type-8 witness -- the latch never reached the sound' + $w } }
        return @{ Pass = $true; Detail = ($hits -join ' | ') + $w }
      } }
    @{ Kind = 'LogCount';   Name = 'no mCreateParams.mpContent assert (CgsVoiceWrapper.cpp:44)'
       Pattern = 'mCreateParams\.mpContent'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no "Content not yet created!" assert (the bank is live)'
       Pattern = 'Content not yet created'; Max = 0 }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
