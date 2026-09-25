# FX-CRASHVFX (crash parity 2026-09-25, item 2) -- glass smashes in a crash, and its debris FALLS, BOUNCES and
# SETTLES, live.
#
# EffectsModule::HandleGlassSmashEventsForAllCars @0x82297420 drains the deformation system's smashed panes: a
# crashing race car's pane (or a traffic car's) gets a glass debris burst (BurstAreaEmitParticles @0x82292160 into
# eDebrisArray_Glass) and up to three 'Glass_shattering' LION effects. In a crash the particle module runs its
# reduced-frame-rate arm (render data 0x40, BrnGameModule::DoDispatch 86fb92b2), so the debris simulation (858d86d7)
# collides each piece with the crash triangle cache: it falls, bounces 2..5 times and then lies still (a piece with
# no bounces left is no longer integrated -- BrnDebrisArrayLite's bucket integrator sub_82C08410).
#
# The drive is the standard wall shot (crash_sweep_batch -Headings 225 -Speeds 70). The witnesses (NOT IN THE X360
# BINARY): [glass] (BRN_GLASS_DIAG=1: one line per glass event the drain reads) and [debris-sim] (BRN_DEBRIS_DIAG=1:
# the newest live piece tracked from its spawn -- position, velocity, spin, bounces left). NO FRAME DUMP.
# The [debris-sim] lines grouped into tracks (one per "(NEW)" pick). A script block, captured by each check's
# closure, because run_case evaluates this file in a child scope (a function would be gone by check time).
$GetDebrisTracks = {
  param($lines)
  $tracks = @(); $cur = $null
  foreach ($line in $lines) {
    if ($line -notmatch '^\[debris-sim\] f=(?<f>\d+) t=(?<t>[0-9.]+) dt=[0-9.]+ crash=(?<c>\d) jobs=-?\d+ integrated=\d+ collide=(?<col>\d+) bounces=(?<b>\d+)(?: cache=(?<cache>\d))? \| array=(?<a>\d+) #(?<i>\d+) age=(?<age>-?[0-9.]+) pos=\((?<px>-?[0-9.]+),(?<py>-?[0-9.]+),(?<pz>-?[0-9.]+)\) vel=\((?<vx>-?[0-9.]+),(?<vy>-?[0-9.]+),(?<vz>-?[0-9.]+)\) angle=(?<ang>-?[0-9.]+) left=(?<left>\d+)(?<new> \(NEW\))?') { continue }
    $inv = [Globalization.CultureInfo]::InvariantCulture
    $p = [pscustomobject]@{
      F = [int]$Matches.f; Crash = [int]$Matches.c; Collide = [int]$Matches.col; Bounces = [int]$Matches.b; Cache = [int]$Matches.cache
      Array = [int]$Matches.a; Index = [int]$Matches.i; Left = [int]$Matches.left
      X = [double]::Parse($Matches.px, $inv); Y = [double]::Parse($Matches.py, $inv); Z = [double]::Parse($Matches.pz, $inv)
      VY = [double]::Parse($Matches.vy, $inv); Angle = [double]::Parse($Matches.ang, $inv)
    }
    if ($Matches.new) { $cur = [System.Collections.ArrayList]::new(); $tracks += ,$cur }
    if ($null -ne $cur) { [void]$cur.Add($p) }
  }
  return ,$tracks
}

@{
  Name    = 'fxcrashvfx_glass'
  Area    = 'vfx'
  Bug     = 'A crash must smash glass: the pane gets its glass debris (320 pieces per unit area) and its shatter effects, and the debris falls, bounces off the ground and settles under the crash camera.'
  Frames  = $false
  Run     = @{
    Drive           = $true
    MaxSeconds      = 75
    CrashSweep      = '3249.796,-3.7,-1925.404'
    CrashSweepShots = '225:70'
    CrashSweepArm   = 4
  }
  DiagEnv = 'BRN_GLASS_DIAG=1,BRN_DEBRIS_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the sweep fired (the car was placed and launched)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the car crashed (a crash record opened)'; Pattern = '\[crash-exit\] OPENED crash record'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the glass drain runs: no NOT RECONSTRUCTED announcement'
       Pattern = 'NOT RECONSTRUCTED: EffectsModule::HandleGlassSmashEventsForAllCars'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'a smashed pane was handled and burst into glass debris ([glass] handled=1 emit=1)'
       Pattern = '^\[glass\] event .* handled=1 emit=1 '; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the debris simulation picked up a GLASS piece (array 4)'
       Pattern = '^\[debris-sim\] .* \| array=4 #\d+ .* \(NEW\)$'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'in crash mode the debris jobs get the crash triangle cache (cache=1) and collide with it'
       Pattern = '^\[debris-sim\] .* crash=1 .* collide=[1-9]\d* bounces=\d+ cache=1 '; Min = 1 }
    @{ Kind = 'Script';   Name = 'a tracked glass piece FALLS: its position changes and gravity drives its vel.y down'; Script = {
        param($ctx)
        $tracks = & $GetDebrisTracks $ctx.LogLines
        $moved = 0; $first = ''
        foreach ($t in $tracks) {
          if ($t.Count -lt 2 -or $t[0].Array -ne 4) { continue }
          $fell = $false
          for ($k = 1; $k -lt $t.Count; $k++) {
            if ($t[$k].Left -eq $t[$k - 1].Left -and $t[$k].VY -lt $t[$k - 1].VY -and
                ([math]::Abs($t[$k].Y - $t[$k - 1].Y) + [math]::Abs($t[$k].X - $t[$k - 1].X) + [math]::Abs($t[$k].Z - $t[$k - 1].Z)) -gt 0.001) { $fell = $true; break }
          }
          if ($fell) { $moved++; if (-not $first) { $first = "#$($t[0].Index) from f$($t[0].F) y=$($t[0].Y) vy=$($t[0].VY)" } }
        }
        return @{ Pass = ($moved -gt 0); Detail = "$moved of $(@($tracks | Where-Object { $_.Count -gt 0 -and $_[0].Array -eq 4 }).Count) tracked glass pieces fell; first: $first" }
      }.GetNewClosure() }
    @{ Kind = 'Script';   Name = 'it BOUNCES in crash mode: a tracked glass piece spends a bounce (left drops) with collision on'; Script = {
        param($ctx)
        $tracks = & $GetDebrisTracks $ctx.LogLines
        $bounced = 0; $first = ''
        foreach ($t in $tracks) {
          if ($t.Count -lt 2 -or $t[0].Array -ne 4) { continue }
          for ($k = 1; $k -lt $t.Count; $k++) {
            if ($t[$k].Left -lt $t[$k - 1].Left -and $t[$k].Crash -eq 1) { $bounced++; if (-not $first) { $first = "#$($t[0].Index) f$($t[$k].F) left $($t[$k - 1].Left)->$($t[$k].Left) y=$($t[$k].Y)" }; break }
          }
        }
        return @{ Pass = ($bounced -gt 0); Detail = "$bounced tracked glass piece(s) bounced; first: $first" }
      }.GetNewClosure() }
    @{ Kind = 'Script';   Name = 'it SETTLES: a tracked glass piece with no bounces left keeps its position on later lines'; Script = {
        param($ctx)
        $tracks = & $GetDebrisTracks $ctx.LogLines
        $settled = 0; $first = ''
        foreach ($t in $tracks) {
          if ($t.Count -lt 2 -or $t[0].Array -ne 4) { continue }
          for ($k = 0; $k -lt $t.Count - 1; $k++) {
            if ($t[$k].Left -ne 0) { continue }
            $still = $true; $later = 0
            for ($j = $k + 1; $j -lt $t.Count; $j++) {
              $later++
              if (([math]::Abs($t[$j].Y - $t[$k].Y) + [math]::Abs($t[$j].X - $t[$k].X) + [math]::Abs($t[$j].Z - $t[$k].Z)) -gt 0.0005) { $still = $false; break }
            }
            if ($still -and $later -gt 0) { $settled++; if (-not $first) { $first = "#$($t[0].Index) at ($($t[$k].X),$($t[$k].Y),$($t[$k].Z)) from f$($t[$k].F), still for $later more line(s)" } }
            break
          }
        }
        return @{ Pass = ($settled -gt 0); Detail = "$settled tracked glass piece(s) settled; first: $first" }
      }.GetNewClosure() }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
