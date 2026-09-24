# FX-VOICEPOOL (crash parity 2026-09-24) -- crash voices must FINISH, live.
#
# Before the fix no crash voice ever finished on PC: CollisionEffect::GetPitch / GetGain (0x826881B0 /
# 0x82688138) read the collision CONTROL's dynamic-mixer endpoint, which the mix map never connects,
# instead of effect+0x34 = the EFFECT's own EffectBase::mpDynamicMixIo. Pitch 0 froze every crash
# splice's clock (SpliceSample::Update 0x826A3B74..0x826A3BAC: time += pitch * dt never reaches
# (mfBasePitch / mLocPitch) * mfEnvLength), gain 0 made it silent, the seven collision states filled
# after seven impacts and every later impact was voiced only by evicting a holder it outranked.
# Pre-fix witness (temporary prints): scratch/bugtest/runs/fxvoicepool_diag/20260924_153539 --
# `effDmix=1 ctlDmix=0 ... pcPitch=0 pcGain=0 effPitchOut=4096`, 90 crash-splice ticks at pitch 0.
#
# The drive is FX-CRASHSND2's deterministic three-shot sweep (quay rocks at 44 m/s, the harbour
# slope, 53 m/s): dozens of impacts, glass, hinges, detached parts.
#
# The witnesses (BRN_COLLISION_AUDIO_DIAG, NOT IN THE X360 BINARY):
#   [collision-audio] play pipeline=.. bin=.. size=.. sample=..      (an impact took a state; cap 64)
#   [collision-audio] voice finished sample=.. bin=.. attached=.. now=.. pitch=.. gain=..
#                                               (CollisionEffect::ProcessUpdate: IsPlaying went false)
#   [collision-audio] detach finished lifetime=<1 impact|2 scrape> sample=.. attached=.. now=..
#                                               (CollisionControl::UpdateParams releases the state)
#   [collision-audio] no free state ...          (all seven states held; the console has the same 7)
@{
  Name    = 'fxvoicepool'
  Area    = 'sound'
  Bug     = 'A crash voice must finish -- its splice runs at the effect endpoint''s pitch to the end of its envelope -- so the collision state detaches and later impacts get a voice.'
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
    @{ Kind = 'LogCount';   Name = 'impacts were voiced (a collision state took a sample)'
       Pattern = '^\[collision-audio\] play pipeline='; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'a crash voice FINISHED at a live pitch (its splice reached the end of its envelope)'
       Pattern = '^\[collision-audio\] voice finished sample=-?\d+ bin=-?\d+ attached=\S+ now=\S+ pitch=(?!0 )\S+ '; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'a finished impact released its collision state (CollisionControl::UpdateParams)'
       Pattern = '^\[collision-audio\] detach finished lifetime=1 '; Min = 1 }
    @{ Kind = 'Script';     Name = 'the pool recycles: a finished impact freed a state before the 9th impact was voiced'; Script = {
        param($ctx)
        $plays = 0; $freedBeforeNinth = 0; $finishedDetaches = 0
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[collision-audio\] play pipeline=') {
            $plays++
            if ($plays -eq 9) { $freedBeforeNinth = $finishedDetaches }
          }
          elseif ($l -match '^\[collision-audio\] detach finished lifetime=1 ') { $finishedDetaches++ }
        }
        return @{ Pass = ($plays -ge 9 -and $freedBeforeNinth -ge 1)
                  Detail = ('{0} impacts voiced; {1} impact states released by a finished voice before the 9th, {2} in all' -f $plays, $freedBeforeNinth, $finishedDetaches) }
      } }
    @{ Kind = 'Script';     Name = 'no crash voice leaks its state: every voiced impact but the (at most) seven still held at the end finished'; Script = {
        param($ctx)
        $plays = 0; $finished = 0; $refusals = 0
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[collision-audio\] play pipeline=') { $plays++ }
          elseif ($l -match '^\[collision-audio\] voice finished ') { $finished++ }
          elseif ($l -match '^\[collision-audio\] no free state ') { $refusals++ }
        }
        return @{ Pass = ($plays -gt 0 -and $finished -ge ($plays - 7))
                  Detail = ('{0} impacts voiced (the play print stops at 64), {1} voices finished, {2} full-pool refusals (print cap 32)' -f $plays, $finished, $refusals) }
      } }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
