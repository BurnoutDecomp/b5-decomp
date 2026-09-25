# FX-CRASHVFX C2 (crash parity 2026-09-25) -- THE JUMP-LANDING SPARKS, live.
#
# When a car lands a jump fast, BrnEffects::JumpStateMachine (ticked per active car by ActiveRaceCarData::Tick) moves
# through Landed into FiringSparks and calls FireWheelSparks @0x82299670: with both REAR wheels on the ground it draws
# two points on the rear axle and hands each to EffectsModule::FireJumpSparks @0x822969E0. That gates on the surface's
# spark scale (visualfxsurface +0x54 >= 0.01) and on the car's speed along the ground (> 4.4694443 m/s), frames the
# shower on the ground normal and the car's slide, and posts one DoSparkShower with gSparkShowerControllerJumpSparks
# (unk_82CDB1E0, world-grinding sparks). Before C2 FireWheelSparks announced itself PARKED (its callee had no body),
# so a landing threw dust and debris but never a spark.
#
# The drive is FxDirectorStuntJumpLive's shot (the one FxCrashVfxDebrisSimLive also takes): one seat 40 m before
# signature jump 4030 at 55.8 m/s; the car lands well above 14 m/s.
# The witnesses (NOT IN THE X360 BINARY, default off, capped):
#   [jump]         BRN_JUMP_DIAG=1 -- the jump machine's state transitions (11 = FiringSparks);
#   [jump-sparks]  BRN_JUMP_DIAG=1 -- one line per FireJumpSparks call: its surface-scale or speed verdict, or the
#                  shower it posted (count, lerp, where);
#   [spark]        BRN_SPARK_DIAG=1 -- the spark ladder; shower= counts the sparks the shower consumer spawned.
# NO FRAME DUMP.
@{
  Name    = 'fxcrashvfx_jump_sparks'
  Area    = 'vfx'
  Bug     = 'A fast jump landing must throw sparks off the rear axle: JumpStateMachine::FireWheelSparks has to run (not announce itself PARKED) and EffectsModule::FireJumpSparks has to post its DoSparkShower.'
  Frames  = $false
  Run     = @{
    Drive            = $true
    MotionProbe      = $true
    MaxSeconds       = 60
    SkipIntro        = $true
    AcceptGap        = 1.0
    CrashSweep       = '3026.55,-8.9,-294.9'
    CrashSweepShots  = '3026.55/-8.9/-294.9/1.45:55.8'
    CrashSweepSettle = 600
  }
  DiagEnv = 'BRN_JUMP_DIAG=1,BRN_SPARK_DIAG=1'
  Checks  = @(
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'the sweep fired (the car was seated on the approach)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'FireWheelSparks runs: no PARKED announcement'
       Pattern = '\[jump\] FireWheelSparks @0x82299670 PARKED'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'the jump machine reached FiringSparks (state 11)'; Pattern = '^\[jump\] state \d+ -> 11$'; Min = 1 }
    @{ Kind = 'Script'; Name = 'the landing posted jump-spark showers (FireJumpSparks -> DoSparkShower with a count)'; Script = {
        param($ctx)
        $rx = '^\[jump-sparks\] t=\S+ surface (?<s>\d+) scale \S+ speed (?<v>\S+) -> DoSparkShower count (?<n>\d+) '
        $showers = @($ctx.LogLines | Where-Object { $_ -match $rx })
        $verdicts = @($ctx.LogLines | Where-Object { $_ -match '^\[jump-sparks\] ' })
        $sparks = 0
        foreach ($l in $showers) { [void]($l -match $rx); $sparks += [int64]$Matches.n }
        $first = if ($showers.Count) { $showers[0] } else { '' }
        @{ Pass = ($showers.Count -gt 0 -and $sparks -gt 0)
           Detail = "$($showers.Count) shower(s) of $($verdicts.Count) FireJumpSparks call(s), $sparks spark(s) asked; first: $first" }
      } }
    @{ Kind = 'Script'; Name = 'the showers reached the spark system: the ladder''s shower= count rises after the first jump shower'; Script = {
        param($ctx)
        $lines = $ctx.LogLines
        $first = -1
        for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -match '^\[jump-sparks\] .* -> DoSparkShower count [1-9]') { $first = $i; break } }
        if ($first -lt 0) { return @{ Pass = $false; Detail = 'no jump shower with a count' } }
        $before = 0; $after = 0
        for ($i = 0; $i -lt $lines.Count; $i++) {
          if ($lines[$i] -match '^\[spark\] prod\{.* shower=(?<s>\d+)\}') {
            if ($i -lt $first) { $before = [int64]$Matches.s } elseif ([int64]$Matches.s -gt $after) { $after = [int64]$Matches.s }
          }
        }
        @{ Pass = ($after -gt $before); Detail = "shower sparks before the first jump shower $before, after it $after" }
      } }
  )
}
