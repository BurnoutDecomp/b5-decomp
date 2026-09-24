# FX-CRASHSND2 item 1 (crash parity 2026-09-24) -- the collision-sound InputCollision builders, live.
#
# The console builders (regular sub_826D3850, prop sub_826E8B20) take every contact's orientation
# from the collision manager's race-car cache (MapPositionToOrientation @0x8269F418 ->
# MapPositionToOrientationUsingBox @0x8269ED18 on the cached transform and deformed box, which
# RaceCarCache::Update @0x826BF478 refreshes at the head of UpdateResolver from the deformation
# output), store the materials with a bare `extsw` (no 0x2000000000 bit), and map traffic by size.
# Before the fix every collision was Front, the cache had no Update, and a race car's contact with
# a prop carried material B = 0x2000000001, so it walked the bin cache and matched nothing -- the
# 15 "gates=0/..." regular rejects of fxcrashsnd_bin_cache/20260924_112457 -- where the console's
# SelectBin exits at `cmpldi 1` (material Nothing) and the prop pipeline voices the contact.
#
# The drive is that run's deterministic three-shot sweep (quay rocks at 44 m/s, the harbour slope,
# 53 m/s), so the same contacts happen and the counts compare.
#
# The witnesses (BRN_COLLISION_AUDIO_DIAG, NOT IN THE X360 BINARY):
#   [collision-audio] race-car cache car=<i> active pos=(..) box=(min)..(max) com=(..)
#   [collision-audio] skip pipeline=<p> second material 1 mat0=0x..
#   [collision-audio] reject pipeline=<p> bins=<n> gates=<10 counters> mat=<hi:lo>/<hi:lo> ...
#   [collision-audio] resolve inputs=<n> outputs=<n> props=<n> orient=<o0>,<o1>,..
#   [collision-audio] play pipeline=<p> bin=<i> size=<s> sample=<id> orient=<o> impulse=..
@{
  Name    = 'fxcrashsnd2_builders'
  Area    = 'sound'
  Bug     = 'Collision sounds must take their orientation from the race-car cache (RaceCarCache::Update + MapPositionToOrientation) and store the materials without the phantom 0x2000000000 bit, so a car-vs-prop contact leaves the regular pipeline at SelectBin''s material-1 exit.'
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
    @{ Kind = 'Script';     Name = 'RaceCarCache::Update filled the player car with a real deformed box'; Script = {
        param($ctx)
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[collision-audio\] race-car cache car=(?<c>\d+) active pos=\([^)]*\) box=\((?<x0>-?[\d.]+),(?<y0>-?[\d.]+),(?<z0>-?[\d.]+)\)\.\.\((?<x1>-?[\d.]+),(?<y1>-?[\d.]+),(?<z1>-?[\d.]+)\)') {
            $ok = ([double]$Matches.x1 -gt [double]$Matches.x0 -and [double]$Matches.y1 -gt [double]$Matches.y0 -and
                   [double]$Matches.z1 -gt [double]$Matches.z0)
            return @{ Pass = $ok; Detail = $l.Trim() }
          }
        }
        return @{ Pass = $false; Detail = 'no [collision-audio] race-car cache line -- the cache was never updated' }
      } }
    @{ Kind = 'LogCount';   Name = 'a race car''s contact with a prop leaves the regular pipeline at SelectBin''s material-1 exit'
       Pattern = '^\[collision-audio\] skip pipeline=0 second material 1 '; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'no regular reject carries a second material of Nothing (1)'
       Pattern = '^\[collision-audio\] reject pipeline=0 .* mat=\d+:\d+/0:1 '; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'unmatched regular collisions (gates=0/...) fewer than the 15 of the pre-fix run'
       Pattern = '^\[collision-audio\] reject pipeline=0 bins=\d+ gates=0/'; Max = 14 }
    @{ Kind = 'LogCount';   Name = 'a contact''s orientation came out other than Front (the cache path is live)'
       Pattern = '^\[collision-audio\] resolve inputs=\d+ outputs=\d+ props=\d+ orient=.*\b(2|4|8|16)\b'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'a resolved collision played a sample'
       Pattern = '^\[collision-audio\] play pipeline=\d+ bin=\d+ size=\d+ sample=\d+'; Min = 1 }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
