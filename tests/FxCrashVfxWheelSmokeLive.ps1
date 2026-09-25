# FX-CRASHVFX C1 (crash parity 2026-09-25) -- THE TYRE SMOKE, live.
#
# BrnEffects::WheelStateMachine::Update @0x82293EB8 runs for every wheel of every active race car every frame
# (EffectsModule::HandleWheels @0x82296C80). A wheel that is attached, has traction and is on the ground, on a car
# that is not crashing (ActiveRaceCarData::mFlags bit 1), moving faster than 1 m/s, resolves its surface's
# visualfxsurface and drives its two smoke layers through HandleSmokeLayer @0x82288E38: particles owed = skid factor
# x distance travelled x particles per metre, and each whole particle is one FireNativeParticle @0x82288C30 ->
# ParticleModule::SpawnWheelSmoke @0x82281AF0 -> a simple particle of the layer's type (3..12, the skid-smoke
# arrays). Before C1 FireNativeParticle announced itself NOT RECONSTRUCTED and no wheel ever smoked; and the crash
# gate read the console's byte offset +0x130, which on x64 is the low half of mID (mFlags sits at +0x138).
#
# The drive: full throttle from a standstill for 4 s (the driven rear wheels spin up: launch smoke), a handbrake turn
# at half left lock for 1.5 s, then the brake. The Teleport is props_hit_speed's straight at (2995, 1.5, -1750); on
# the 2026-09-25 builds the harness re-seats the car on the junkyard-exit road instead ([teleport] ... RE-RESET), and
# the smoke is laid there -- where the drive happens does not matter to this case. GREEN on exe 19:14:48 (b5 C1):
# run 20260925_191508, 67 particles off wheels 2 / 3 (type 3, 1.2 per metre), 0 NaN lines; RED on the pre-C1 exe:
# run 20260925_185820 (the FireNativeParticle announcement, no smoke).
# The witnesses (NOT IN THE X360 BINARY, default off, capped):
#   [wheel-smoke]  BRN_WHEEL_SMOKE_DIAG=1 -- one line per HandleSmokeLayer call that spawned (time, race car, wheel,
#                  layer, type, count, skid factor, travel, particles per metre, what is still owed, where; 48 lines),
#                  one per wheel the crash gate silenced (8), and a running per-car tally (particles spawned, wheel-
#                  frames gated; one line per second of effects time);
#   [simplefx]     BRN_SIMPLEFX_DIAG=1 -- the simple-particle renderer's first draw of each type (its texture).
# The one ALWAYS-ON line, "[wheel-smoke] NaN ACCUMULATOR", is written only when a NaN reaches the spawn loop, where the
# console hangs (0x82289034); this case requires ZERO of them and a sim that keeps stepping after the smoke.
# NO FRAME DUMP.
@{
  Name    = 'fxcrashvfx_wheel_smoke'
  Area    = 'vfx'
  Bug     = 'A sliding tyre must smoke: WheelStateMachine::FireNativeParticle has to spawn the console''s skid-smoke particle through ParticleModule::SpawnWheelSmoke, not announce itself, and the particles must reach the simple-particle renderer.'
  Frames  = $false
  Run     = @{
    Drive          = $true
    MotionProbe    = $true
    MaxSeconds     = 60
    SkipIntro      = $true
    AcceptGap      = 1.0        # harness pump latency, not a game gate
    Teleport       = '2995,1.5,-1750,0'
    ThrottleScript = '0:accel,4:accel+handbrake,5.5:brake,8:none'
    SteerScript    = '0:none,4:left50,5.5:none'
  }
  DiagEnv = 'BRN_WHEEL_SMOKE_DIAG=1,BRN_SIMPLEFX_DIAG=1'
  Checks  = @(
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'FireNativeParticle runs: no NOT RECONSTRUCTED announcement'
       Pattern = 'NOT RECONSTRUCTED: WheelStateMachine::FireNativeParticle'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'ZERO NaN-accumulator lines (the console hangs there, 0x82289034)'
       Pattern = '^\[wheel-smoke\] NaN ACCUMULATOR'; Max = 0 }
    @{ Kind = 'Script'; Name = 'the tyres smoke: HandleSmokeLayer spawned skid-smoke particles (types 3..12) off the sliding wheels'; Script = {
        param($ctx)
        $rx = '^\[wheel-smoke\] t=(?<t>\S+) car (?<c>\d+) wheel (?<w>\d+) layer (?<l>\d+) type (?<type>\d+) spawned (?<n>-?\d+) '
        $rows = @($ctx.LogLines | Where-Object { $_ -match $rx })
        $total = 0; $wheels = @{}; $types = @{}; $bad = 0
        foreach ($line in $rows) {
          [void]($line -match $rx)
          $n = [int]$Matches.n; $type = [int]$Matches.type
          $total += $n; $wheels[$Matches.w] = 1; $types[$type] = 1
          if ($n -lt 1 -or $type -lt 3 -or $type -gt 12) { $bad++ }
        }
        $first = if ($rows.Count) { $rows[0] } else { '' }
        @{ Pass = ($rows.Count -gt 0 -and $bad -eq 0)
           Detail = "$($rows.Count) spawning call(s), $total particle(s), wheels {$(($wheels.Keys | Sort-Object) -join ',')}, types {$(($types.Keys | Sort-Object) -join ',')}, $bad off-range; first: $first" }
      } }
    @{ Kind = 'Script'; Name = 'the smoke is drawn: the simple-particle renderer drew a skid-smoke type with a texture'; Script = {
        param($ctx)
        $draws = @($ctx.LogLines | Where-Object { $_ -match '^\[simplefx\] draw type (?<type>\d+): texture=(?<tex>\S+) ' -and [int]$Matches.type -ge 3 -and [int]$Matches.type -le 12 })
        $textured = @($draws | Where-Object { $_ -notmatch 'texture=(0000000000000000|00000000|\(nil\)|0x0+) ' })
        @{ Pass = ($textured.Count -gt 0); Detail = "$($draws.Count) skid-smoke draw line(s), $($textured.Count) textured: $(($draws | Select-Object -First 3) -join ' | ')" }
      } }
    @{ Kind = 'Script'; Name = 'no hang: the sim kept stepping after the smoke ([motion] samples after the first spawning call)'; Script = {
        param($ctx)
        $lines = $ctx.LogLines
        $first = -1
        for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -match '^\[wheel-smoke\] t=\S+ car \d+ wheel \d+ layer') { $first = $i; break } }
        if ($first -lt 0) { return @{ Pass = $false; Detail = 'no [wheel-smoke] spawning line' } }
        $after = 0
        for ($i = $first + 1; $i -lt $lines.Count; $i++) { if ($lines[$i] -match '^\[motion\] n ') { $after++ } }
        @{ Pass = ($after -ge 10); Detail = "$after [motion] sample(s) (0.5 s apart) after the first spawning call" }
      } }
    @{ Kind = 'Script'; Name = 'the tally agrees: the player car (race car 0) spawned smoke'; Script = {
        param($ctx)
        $tally = @($ctx.LogLines | Where-Object { $_ -match '^\[wheel-smoke\] tally t=\S+ spawned (?<s>(\d+ ){8})gated' })
        if ($tally.Count -lt 1) { return @{ Pass = $false; Detail = 'no [wheel-smoke] tally line' } }
        $last = $tally[$tally.Count - 1]
        [void]($last -match '^\[wheel-smoke\] tally t=\S+ spawned (?<s0>\d+) ')
        @{ Pass = ([int64]$Matches.s0 -gt 0); Detail = "$($tally.Count) tally line(s); last: $last" }
      } }
  )
}
