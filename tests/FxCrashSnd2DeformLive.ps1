# FX-CRASHSND2 item 3 (crash parity 2026-09-24) -- the deformation legs of the collision sound, live.
#
# ARTIST UpdateResolver 0x826F940C..0x826F94B0: after the race-car and traffic contacts, the deformation
# output the sound input carries is turned into collisions -- UpdateGlass @0x826D4850 (a pane cracking or
# smashing), UpdateHingingBodyParts @0x826D44D0 (a door / bonnet / bumper opening, closing, swinging), the
# broken-joint and detached-part queues (a part coming off) -- and so are ContactSpyData's physical car-part
# contacts (a detached part or wheel hitting something). Before the fix none of these reached the collision
# sound.
#
# The drive is the item-1/2 deterministic three-shot sweep (quay rocks at 44 m/s, the harbour slope,
# 53 m/s): the impacts crack and smash glass, swing and tear off panels.
#
# What the console does with them is data: the regular crash-bin table (dumped by the run as
# `[collision-audio] cache entry pipeline=0 N ... action=A`) voices a PlayerCar/AI/traffic vs glass SMASH
# (bin 36, impulse 2.9..3 -- exactly the builders' KF_MAX_IMPULSE 3.0), a LARGE part's detach / hinge
# (bins 25/26/31/34 Detach, 29 HingeOpen, 28/32 HingeClose, 37 Hinging) and a part's contact, and has NO
# bin for a small part's hinge or detach -- so those are rejected at the action gate, on the console too.
# The glass builder culls everything but an unsuppressed smash (0x826BE544..0x826BE564).
#
# The witnesses (BRN_COLLISION_AUDIO_DIAG, NOT IN THE X360 BINARY):
#   [collision-audio] deform queues glass=.. hinge=.. joint=.. detached=.. carpart=..   (queue census)
#   [collision-audio] glass event vehicle=.. part=.. state=<1 crack|2 smash> dontplay=..
#   [collision-audio] deform leg=<glass|hinge|joint|detached|carpart> action=<a> mat=<m0>/<m1> cull=<0|1> ..
#   [collision-audio] cache select ... input=deform      (a deformation input matched an authored bin)
#   [collision-audio] reject ... action=<a>              (walked the bins, none took it)
#   [collision-audio] play pipeline=.. bin=.. ... action=<a> mat=<m0>/<m1>
#   [collision-audio] no free state ... attached=7       (the 7-state pool was full; see the voicing check)
# Actions: 1 Collision, 2 Detach, 4 Hinging, 8 HingeOpen, 16 HingeClose. Materials (hex): 2 PlayerCar,
# 8 TrafficCar, 10 World, 20 BodyPartSmall, 40 BodyPartLarge, 100 GlassSmall, 200 GlassLarge.
@{
  Name    = 'fxcrashsnd2_deform'
  Area    = 'sound'
  Bug     = 'The deformation output (glass smashes, hinged parts, detached parts) and the physical car-part contacts must become collision-sound inputs, as UpdateResolver''s deformation legs make them.'
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
    @{ Kind = 'LogCount';   Name = 'the deformation output reached the sound input (a non-empty queue census)'
       Pattern = '^\[collision-audio\] deform queues '; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'UpdateGlass built glass inputs (PlayerCar vs small / large glass)'
       Pattern = '^\[collision-audio\] deform leg=glass action=1 mat=2/(100|200) '; Min = 1 }
    @{ Kind = 'Script';     Name = 'the glass builder keeps only an unsuppressed SMASH (0x826BE544..0x826BE564)'; Script = {
        param($ctx)
        $cracks = 0; $smashes = 0; $culled = 0; $live = 0
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[collision-audio\] glass event vehicle=01000000 part=\d+ state=1 ') { $cracks++ }
          elseif ($l -match '^\[collision-audio\] glass event vehicle=01000000 part=\d+ state=2 dontplay=0 ') { $smashes++ }
          elseif ($l -match '^\[collision-audio\] deform leg=glass action=1 mat=2/(100|200) cull=1 ') { $culled++ }
          elseif ($l -match '^\[collision-audio\] deform leg=glass action=1 mat=2/(100|200) cull=0 ') { $live++ }
        }
        $crackRule = ($cracks -eq 0) -or ($culled -gt 0)
        $smashRule = (($smashes -gt 0) -eq ($live -gt 0))
        return @{ Pass = ($crackRule -and $smashRule -and ($cracks + $smashes) -gt 0)
                  Detail = ('player glass events: {0} cracks / {1} smashes; glass inputs: {2} culled / {3} live' -f $cracks, $smashes, $culled, $live) }
      } }
    @{ Kind = 'LogCount';   Name = 'a hinged part opened, closed or swung (UpdateHingingBodyParts)'
       Pattern = '^\[collision-audio\] deform leg=hinge action=(4|8|16) '; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'a part came off (the detached-part queue, action Detach)'
       Pattern = '^\[collision-audio\] deform leg=detached action=2 '; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'a detached part touched something (ContactSpyData physical car-part contacts)'
       Pattern = '^\[collision-audio\] deform leg=carpart action=1 '; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the deformation inputs were walked through the authored crash bins (selected or rejected)'
       Pattern = '^\[collision-audio\] (cache select pipeline=0 .* input=deform$|reject pipeline=0 .* action=(2|4|8|16) )'; Min = 1 }
    @{ Kind = 'Script';     Name = 'a deformation collision that matched a bin was voiced -- unless the 7-state pool was full'; Script = {
        param($ctx)
        $selected = 0; $played = 0; $saturated = 0
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[collision-audio\] cache select pipeline=0 .* input=deform$') { $selected++ }
          elseif ($l -match '^\[collision-audio\] play pipeline=0 bin=\d+ size=\d+ sample=\d+ .* (action=(2|4|8|16) |action=1 mat=(2|4|8)/(100|200)$)') { $played++ }
          elseif ($l -match '^\[collision-audio\] no free state .* attached=7 ') { $saturated++ }
        }
        $pass = ($played -gt 0) -or ($selected -eq 0) -or ($saturated -gt 0)
        return @{ Pass = $pass
                  Detail = ('{0} deformation bin selections, {1} voiced, {2} full-pool refusals (voices never finish on PC: see the FX-CRASHSND2 log)' -f $selected, $played, $saturated) }
      } }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
