# FX-CRASHVFX (crash parity 2026-09-25, item 3) -- a crash THROWS THE CAR'S DEBRIS, live.
#
# ParticleModule::HandleFireDebrisBurstEvent @0x8229A660 consumes the burst record ParticleModule::FireDebrisBurst posts
# for a crashing car (EffectsModule::HandleBurstDebris): for each of the four burst arrays (Coloured -- the car's
# paint --, Shiny, Dark, HighDetail) count * scale pieces, each placed in the emitter box, thrown at the camera or
# into a cone, sized, spun and coloured, straight into BrnDebrisArray::SpawnDebris. Before the fix the record reached
# the handler and was dropped with a NOT RECONSTRUCTED announcement. The debris simulation (858d86d7, the crash cache
# 2f19433e, the frame step 49aec128) then makes each piece fall, bounce off the crash cache and settle.
#
# The drive is the standard wall shot (225 deg at 70 m/s). The witnesses (NOT IN THE X360 BINARY, default off):
#   [debris-burst] BRN_DEBRIS_BURST_DIAG=1 -- one line per burst record: pieces per array, how many flew at the camera,
#                  the first piece's position / velocity;
#   [debris-sim]   BRN_DEBRIS_DIAG=1 -- the newest live piece tracked from its spawn (any array: the burst's 0..3 win).
# NO FRAME DUMP.
$GetDebrisTracks = {
  param($lines)
  $tracks = @(); $cur = $null
  foreach ($line in $lines) {
    if ($line -notmatch '^\[debris-sim\] f=(?<f>\d+) t=(?<t>[0-9.]+) dt=[0-9.]+ crash=(?<c>\d) jobs=-?\d+ integrated=\d+ collide=(?<col>\d+) bounces=(?<b>\d+)(?: cache=(?<cache>\d))? \| array=(?<a>\d+) #(?<i>\d+) age=(?<age>-?[0-9.]+) pos=\((?<px>-?[0-9.]+),(?<py>-?[0-9.]+),(?<pz>-?[0-9.]+)\) vel=\((?<vx>-?[0-9.]+),(?<vy>-?[0-9.]+),(?<vz>-?[0-9.]+)\) angle=(?<ang>-?[0-9.]+) left=(?<left>\d+)(?<new> \(NEW\))?') { continue }
    $inv = [Globalization.CultureInfo]::InvariantCulture
    $p = [pscustomobject]@{
      F = [int]$Matches.f; Crash = [int]$Matches.c; Array = [int]$Matches.a; Index = [int]$Matches.i; Left = [int]$Matches.left
      X = [double]::Parse($Matches.px, $inv); Y = [double]::Parse($Matches.py, $inv); Z = [double]::Parse($Matches.pz, $inv)
      VY = [double]::Parse($Matches.vy, $inv)
    }
    if ($Matches.new) { $cur = [System.Collections.ArrayList]::new(); $tracks += ,$cur }
    if ($null -ne $cur) { [void]$cur.Add($p) }
  }
  return ,$tracks
}

@{
  Name    = 'fxcrashvfx_debris_burst'
  Area    = 'vfx'
  Bug     = 'A crash must throw the car''s debris: the burst record becomes painted, shiny, dark and detailed pieces thrown at the camera and in a cone, which fall, bounce and settle.'
  Frames  = $false
  Run     = @{
    Drive           = $true
    MaxSeconds      = 75
    CrashSweep      = '3249.796,-3.7,-1925.404'
    CrashSweepShots = '225:70'
    CrashSweepArm   = 4
  }
  DiagEnv = 'BRN_DEBRIS_BURST_DIAG=1,BRN_DEBRIS_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the sweep fired (the car was placed and launched)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the car crashed (a crash record opened)'; Pattern = '\[crash-exit\] OPENED crash record'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the burst handler runs: no NOT RECONSTRUCTED announcement'
       Pattern = 'NOT RECONSTRUCTED: ParticleModule::HandleFireDebrisBurstEvent'; Max = 0 }
    @{ Kind = 'Script';   Name = 'a burst record spawned pieces into the four arrays ([debris-burst] pieces > 0)'; Script = {
        param($ctx)
        $rows = @($ctx.LogLines | Where-Object { $_ -match '^\[debris-burst\] ' })
        $total = 0; $first = ''
        foreach ($r in $rows) {
          if ($r -match 'pieces=(\d+)/(\d+)/(\d+)/(\d+) atCamera=(\d+)') {
            $n = [int]$Matches[1] + [int]$Matches[2] + [int]$Matches[3] + [int]$Matches[4]; $total += $n
            if (-not $first -and $n -gt 0) { $first = $r }
          }
        }
        return @{ Pass = ($total -gt 0); Detail = "$($rows.Count) burst record(s), $total piece(s); first: $first" }
      }.GetNewClosure() }
    @{ Kind = 'LogCount'; Name = 'the debris simulation picked up a BURST piece (arrays 0..3)'
       Pattern = '^\[debris-sim\] .* \| array=[0-3] #\d+ .* \(NEW\)$'; Min = 1 }
    @{ Kind = 'Script';   Name = 'a tracked burst piece FALLS: its position changes and gravity drives its vel.y down'; Script = {
        param($ctx)
        $tracks = & $GetDebrisTracks $ctx.LogLines
        $moved = 0; $first = ''
        foreach ($t in $tracks) {
          if ($t.Count -lt 2 -or $t[0].Array -gt 3) { continue }
          for ($k = 1; $k -lt $t.Count; $k++) {
            if ($t[$k].Left -eq $t[$k - 1].Left -and $t[$k].VY -lt $t[$k - 1].VY -and
                ([math]::Abs($t[$k].Y - $t[$k - 1].Y) + [math]::Abs($t[$k].X - $t[$k - 1].X) + [math]::Abs($t[$k].Z - $t[$k - 1].Z)) -gt 0.001) {
              $moved++; if (-not $first) { $first = "array $($t[0].Array) #$($t[0].Index) from f$($t[0].F) y=$($t[0].Y) vy=$($t[0].VY)" }; break
            }
          }
        }
        return @{ Pass = ($moved -gt 0); Detail = "$moved tracked burst piece(s) fell; first: $first" }
      }.GetNewClosure() }
    @{ Kind = 'Script';   Name = 'it BOUNCES in crash mode: a tracked burst piece spends a bounce (left drops) with collision on'; Script = {
        param($ctx)
        $tracks = & $GetDebrisTracks $ctx.LogLines
        $bounced = 0; $first = ''
        foreach ($t in $tracks) {
          if ($t.Count -lt 2 -or $t[0].Array -gt 3) { continue }
          for ($k = 1; $k -lt $t.Count; $k++) {
            if ($t[$k].Left -lt $t[$k - 1].Left -and $t[$k].Crash -eq 1) { $bounced++; if (-not $first) { $first = "array $($t[0].Array) #$($t[0].Index) f$($t[$k].F) left $($t[$k - 1].Left)->$($t[$k].Left) y=$($t[$k].Y)" }; break }
          }
        }
        return @{ Pass = ($bounced -gt 0); Detail = "$bounced tracked burst piece(s) bounced; first: $first" }
      }.GetNewClosure() }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
