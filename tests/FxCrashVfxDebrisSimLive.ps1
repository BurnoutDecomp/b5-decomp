# FX-CRASHVFX (crash parity 2026-09-25) -- THE DEBRIS SIMULATION, live: a spawned debris piece MOVES.
#
# ParticleModule::DispatchThreadUpdate @0x8229C5F0 calls BeginSimulateDebris @0x82289A98 straight after the
# inter-thread event drain: every array's expired buckets are recycled, then one DebrisUpdateJob per array with live
# buckets integrates every live piece (DebrisUpdateJob::Execute @0x82C08298 -> BrnDebrisArrayLite::Update
# @0x82C08B58 -> the per-bucket integrator sub_82C08410): gravity (0, -9.8, 0) and drag on the velocity, the fused
# position step, the spin advanced by the distance moved, and in crash mode a bounce off the crash triangle cache.
# Before the fix none of it ran -- BeginSimulateDebris / EndSimulateDebris were announced NOT REPRODUCED and
# BrnDebrisArrayLite.cpp was an unmounted assert stub -- so a debris piece stayed exactly where SpawnDebris put it.
#
# The drive is FxDirectorStuntJumpLive's shot: one seat 40 m before signature jump 4030 at 55.8 m/s. The landing after
# more than a second of air at more than 17.9 m/s fires JumpStateMachine::FireWheelDebris (20..40 dark debris pieces
# off the rear wheels' contact line) -- the one debris source live on this build.
#
# The witness (BRN_DEBRIS_DIAG=1, NOT IN THE X360 BINARY, capped at 600 lines):
#   [debris-sim] f=<frame> t=.. dt=.. crash=<0|1> jobs=<n> integrated=<n> collide=<n> bounces=<n> |
#                array=<a> #<i> age=.. pos=(x,y,z) vel=(x,y,z) angle=.. left=<bounces left>[ (NEW)]
# One piece is tracked from its pick (NEW) until it is gone; a piece that MOVES shows a changing pos, a vel.y that
# gravity drives down, and a spin angle that advances.
# NO FRAME DUMP (the 2026-09-25 disk rule).
@{
  Name    = 'fxcrashvfx_debris_sim'
  Area    = 'vfx'
  Bug     = 'Debris must move once spawned: ParticleModule::BeginSimulateDebris and the debris update job have to integrate every live piece (gravity, drag, spin, bounces), not leave it hanging where it spawned.'
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
  DiagEnv = 'BRN_DEBRIS_DIAG=1,BRN_JUMP_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the sweep fired (the car was seated on the approach)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'BeginSimulateDebris runs: no NOT RECONSTRUCTED announcement'
       Pattern = 'NOT RECONSTRUCTED: ParticleModule::DispatchThreadUpdate''s BeginSimulateDebris'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'EndSimulateDebris runs: no NOT RECONSTRUCTED announcement'
       Pattern = 'NOT RECONSTRUCTED: ParticleModule::RenderFullResParticles'' EndSimulateDebris'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'the debris witness is armed'; Pattern = '^\[debris-sim\] probe ARMED'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'debris was spawned and picked up by the simulation (a tracked piece)'
       Pattern = '^\[debris-sim\] .* \(NEW\)$'; Min = 1 }
    @{ Kind = 'Script';   Name = 'a tracked piece MOVES: its position changes, gravity drives its vel.y down, its spin advances'; Script = {
        param($ctx)
        $inv = [Globalization.CultureInfo]::InvariantCulture
        $rx = '^\[debris-sim\] f=(?<f>\d+) .* integrated=(?<n>\d+) .* \| array=(?<a>\d+) #(?<i>\d+) age=(?<age>[-\d.]+) pos=\((?<px>[-\d.]+),(?<py>[-\d.]+),(?<pz>[-\d.]+)\) vel=\((?<vx>[-\d.]+),(?<vy>[-\d.]+),(?<vz>[-\d.]+)\) angle=(?<ang>[-\d.]+) left=(?<left>\d+)(?<new> \(NEW\))?$'
        $episodes = @(); $cur = $null
        foreach ($l in $ctx.LogLines) {
          if ($l -notmatch $rx) { continue }
          $s = [pscustomobject]@{ f = [int]$Matches.f; py = [double]::Parse($Matches.py, $inv); px = [double]::Parse($Matches.px, $inv)
                                  pz = [double]::Parse($Matches.pz, $inv); vy = [double]::Parse($Matches.vy, $inv)
                                  ang = [double]::Parse($Matches.ang, $inv); age = [double]::Parse($Matches.age, $inv) }
          if ($Matches.new) { $cur = [System.Collections.ArrayList]@(); $episodes += ,$cur }
          if ($null -ne $cur) { [void]$cur.Add($s) }
        }
        $moving = 0; $first = ''
        foreach ($e in $episodes) {
          if ($e.Count -lt 2) { continue }
          $a = $e[0]; $b = $e[$e.Count - 1]
          $d = [Math]::Sqrt([Math]::Pow($b.px - $a.px, 2) + [Math]::Pow($b.py - $a.py, 2) + [Math]::Pow($b.pz - $a.pz, 2))
          if ($d -gt 0.05 -and $b.vy -lt $a.vy -and $b.ang -ne $a.ang) {
            $moving++
            if (-not $first) { $first = ('age {0:F2}->{1:F2}: y {2:F2}->{3:F2}, vel.y {4:F2}->{5:F2}, moved {6:F2} m, angle {7:F2}->{8:F2}' -f $a.age, $b.age, $a.py, $b.py, $a.vy, $b.vy, $d, $a.ang, $b.ang) }
          }
        }
        return @{ Pass = ($moving -gt 0); Detail = "$moving of $($episodes.Count) tracked pieces moved; first: $first" }
      } }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
