# FX-CRASHSND item 2 (crash parity 2026-09-24) -- the collision-audio bin lookup caches, live.
#
# CollisionStateManager::ResourcesAreReady @0x826D3788 builds maBinLoopupCache[0] from the crash-bin
# list (0x826D37C0, Build<crashbinlist, crashbin> @0x826A85F8) and [1] from the props crash-bin list
# (0x826D37D0, Build<propscrashbinlist, propscrashbin> @0x826A8710); Prepare @0x826F8B78 state 2
# constructs the three collision Contents once that is done; SelectBin (@0x826A97E8 / @0x826A8828)
# resolves every collision's bin through the cache of its pipeline. Before the fix none of that ran
# (Build had no caller and no mount line).
#
# The witnesses (BRN_COLLISION_AUDIO_DIAG, NOT IN THE X360 BINARY):
#   [collision-audio] bin caches built regular=<cache>/<list> prop=<cache>/<list>
#   [collision-audio] cache select pipeline=<p> entry=<i>/<n> forward|reverse matA=.. matB=..
#   [collision-audio] play pipeline=<p> bin=<i> size=<s> sample=<id> ...
# The drive: the same seats FxCrashSndResetStingLive uses (the quay rocks at 44 m/s, the harbour
# slope) with the throttle held -- a run that crashes the player car into the world repeatedly.
@{
  Name    = 'fxcrashsnd_bin_cache'
  Area    = 'sound'
  Bug     = 'Collision sounds must resolve their crash bin through the lookup caches ResourcesAreReady builds, with the collision Contents constructed in Prepare.'
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
  DiagEnv = 'BRN_COLLISION_AUDIO_DIAG=1,BRN_HUD_SOUND_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'the sweep fired (the car was placed)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'Script';     Name = 'ResourcesAreReady built both caches, one entry per listed bin'; Script = {
        param($ctx)
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[collision-audio\] bin caches built regular=(?<rc>\d+)/(?<rl>\d+) prop=(?<pc>\d+)/(?<pl>\d+)') {
            $ok = ([int]$Matches.rc -gt 0 -and [int]$Matches.rc -eq [int]$Matches.rl -and
                   [int]$Matches.pc -gt 0 -and [int]$Matches.pc -eq [int]$Matches.pl)
            return @{ Pass = $ok; Detail = $l.Trim() }
          }
        }
        return @{ Pass = $false; Detail = 'no [collision-audio] bin caches built line -- ResourcesAreReady never built them' }
      } }
    @{ Kind = 'LogCount';   Name = 'crash collisions resolved their bin through the cache'
       Pattern = '^\[collision-audio\] cache select pipeline=\d+ entry=\d+/\d+ (forward|reverse) '; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'a resolved collision played a sample'
       Pattern = '^\[collision-audio\] play pipeline=\d+ bin=\d+ size=\d+ sample=\d+'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'no "Content not yet created" (the Contents Prepare constructs are live)'
       Pattern = 'Content not yet created'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no mCreateParams.mpContent assert'
       Pattern = 'mCreateParams\.mpContent'; Max = 0 }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
