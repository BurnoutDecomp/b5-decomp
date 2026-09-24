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
#   [collision-audio] voice attached sample=.. pipeline=.. action=.. owners=A/B impulse=.. fatality=..
#                    ... duck=<mixer input 1> azimuth=<0|1>
#                                               (CollisionEffect::Attach @0x826F8218: the ducking the crash
#                                                sends the mix, re-derived here from CalculateIntensity's
#                                                console constants; second piece of this lane)
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
    @{ Kind = 'Script';     Name = 'every crash voice ducks the mix by the console''s amount (Attach 0x826F8218 + CalculateIntensity 0x82688240) and keeps its azimuth unless it starts a fatality'; Script = {
        param($ctx)
        # [collision-audio] voice attached sample=.. pipeline=P action=A owners=a/b impulse=x fatality=F ... duck=D azimuth=Z
        $n = 0; $bad = 0; $live = 0; $first = ''
        foreach ($l in $ctx.LogLines) {
          if ($l -notmatch '^\[collision-audio\] voice attached sample=-?\d+ pipeline=(\d+) action=(\d+) owners=(\d+)/(\d+) impulse=(\S+) fatality=(\d+) .* duck=(-?\d+) azimuth=(\d)') { continue }
          $n++
          $pipe = [int]$Matches[1]; $act = [int]$Matches[2]; $a = [int]$Matches[3]; $b = [int]$Matches[4]
          $x = [double]::Parse($Matches[5], [Globalization.CultureInfo]::InvariantCulture)
          $fat = [int]$Matches[6]; $duck = [int]$Matches[7]; $az = [int]$Matches[8]
          # CalculateIntensity: 12 * x for a prop hit; (100 - 25) * x + 25 for race car vs world/race car/traffic
          # or traffic vs anything (flt_82F2CEC4..flt_82F2CED4); 0 otherwise or for a non-Collision action.
          $ci = 0.0
          if ($act -eq 1) {
            if ($pipe -eq 1) { $ci = 12.0 * $x }
            elseif (($a -eq 1 -and $b -le 2) -or $a -eq 2) { $ci = 75.0 * $x + 25.0 }
          }
          $want = $ci * 327.67001
          if ($want -le 0) { $want = 0 }
          if ($want -gt 32767) { $want = 32767 }
          $want = [math]::Truncate($want)
          $azWant = if ($fat -eq 2) { 0 } else { 1 }
          if ([math]::Abs($duck - $want) -gt 2 -or $az -ne $azWant) { $bad++; if (-not $first) { $first = $l } }
          if ($duck -gt 0) { $live++ }
        }
        $detail = ('{0} voice starts checked, {1} ducking the mix, {2} off the console value' -f $n, $live, $bad)
        if ($first) { $detail += (' -- first: ' + $first) }
        return @{ Pass = ($n -gt 0 -and $bad -eq 0 -and $live -gt 0); Detail = $detail }
      } }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
