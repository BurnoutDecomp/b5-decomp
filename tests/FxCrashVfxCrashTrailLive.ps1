# FX-CRASHVFX (crash parity 2026-09-25, item 4) -- a crashing car SHEDS A DEBRIS TRAIL, live.
#
# EffectsModule::HandleCrashingTrail @0x82290D30 runs for every crashing race car on every sim step: per piece type
# (Coloured -- the car's paint --, Shiny, Dark, HighDetail, Glass, then impact smoke) the car's accumulator gains
# distance slid x Trail_MasterEmissionRate x the speed factor, and its whole part is emitted as pieces spread along the
# slide, each thrown off the car's box into BrnDebrisArray::SpawnDebris (or SpawnSimple, eParticleArray_ImpactSmoke).
# Before the fix the handler announced itself NOT RECONSTRUCTED right after the crash opened and the wreck shed nothing.
#
# The drive is the standard wall shot (225 deg at 70 m/s). The witnesses (NOT IN THE X360 BINARY, default off):
#   [crash-trail]  BRN_CRASH_TRAIL_DIAG=1 -- one line per call that emitted: the car, the distance it slid this step,
#                  the speed factor, the pieces per type and the first piece's spot;
#   [debris-sim]   BRN_DEBRIS_DIAG=1 -- the newest live debris piece tracked from its spawn.
# NO FRAME DUMP.
$GetTrailRows = {
  param($lines)
  $inv = [Globalization.CultureInfo]::InvariantCulture
  $rows = @()
  foreach ($line in $lines) {
    if ($line -notmatch '^\[crash-trail\] car=(?<car>\d+) dist=(?<d>-?[0-9.]+|-?nan|-?inf) factor=(?<f>-?[0-9.]+|-?nan|-?inf) pieces=(?<p0>\d+)/(?<p1>\d+)/(?<p2>\d+)/(?<p3>\d+)/(?<p4>\d+) smoke=(?<s>\d+) first=\((?<x>[^,]+),(?<y>[^,]+),(?<z>[^)]+)\)') { continue }
    $rows += [pscustomobject]@{
      Car = [int]$Matches.car; Dist = $Matches.d; Factor = $Matches.f
      Pieces = @([int]$Matches.p0, [int]$Matches.p1, [int]$Matches.p2, [int]$Matches.p3, [int]$Matches.p4, [int]$Matches.s)
      X = $Matches.x; Y = $Matches.y; Z = $Matches.z; Line = $line
    }
  }
  return ,$rows
}

@{
  Name    = 'fxcrashvfx_crash_trail'
  Area    = 'vfx'
  Bug     = 'A crashing car must shed its debris trail: painted, shiny, dark, detailed and glass pieces and impact smoke spread along the slide, more the faster it slides.'
  Frames  = $false
  Run     = @{
    Drive           = $true
    MaxSeconds      = 75
    CrashSweep      = '3249.796,-3.7,-1925.404'
    CrashSweepShots = '225:70'
    CrashSweepArm   = 4
  }
  DiagEnv = 'BRN_CRASH_TRAIL_DIAG=1,BRN_DEBRIS_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the sweep fired (the car was placed and launched)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the car crashed (a crash record opened)'; Pattern = '\[crash-exit\] OPENED crash record'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the trail handler runs: no NOT RECONSTRUCTED announcement'
       Pattern = 'NOT RECONSTRUCTED: EffectsModule::HandleCrashingTrail'; Max = 0 }
    @{ Kind = 'Script';   Name = 'the crashing car shed trail pieces ([crash-trail] pieces > 0)'; Script = {
        param($ctx)
        $rows = & $GetTrailRows $ctx.LogLines
        $sum = @(0, 0, 0, 0, 0, 0); $cars = @{}
        foreach ($r in $rows) { for ($k = 0; $k -lt 6; $k++) { $sum[$k] += $r.Pieces[$k] }; $cars["$($r.Car)"] = 1 }
        $total = ($sum | Measure-Object -Sum).Sum
        $first = if ($rows.Count) { $rows[0].Line } else { '' }
        return @{ Pass = ($total -gt 0)
                  Detail = "$($rows.Count) emitting call(s) from car(s) $((@($cars.Keys) | Sort-Object) -join ','); pieces Coloured/Shiny/Dark/HighDetail/Glass/smoke = $($sum -join '/'); first: $first" }
      }.GetNewClosure() }
    @{ Kind = 'Script';   Name = 'every emitting call slid a finite distance > 0 with a speed factor in [0, 1] (the two fsel clamp)'; Script = {
        param($ctx)
        $inv = [Globalization.CultureInfo]::InvariantCulture
        $rows = & $GetTrailRows $ctx.LogLines
        $bad = @()
        foreach ($r in $rows) {
          $d = 0.0; $f = 0.0
          $okD = [double]::TryParse($r.Dist, [Globalization.NumberStyles]::Float, $inv, [ref]$d) -and $d -gt 0.0
          $okF = [double]::TryParse($r.Factor, [Globalization.NumberStyles]::Float, $inv, [ref]$f) -and $f -ge 0.0 -and $f -le 1.0
          if (-not ($okD -and $okF)) { $bad += $r.Line }
        }
        return @{ Pass = ($rows.Count -gt 0 -and $bad.Count -eq 0)
                  Detail = "$($rows.Count) row(s), $($bad.Count) out of range$(if ($bad.Count) { '; first: ' + $bad[0] })" }
      }.GetNewClosure() }
    @{ Kind = 'Script';   Name = 'the trail''s pieces have finite spots'; Script = {
        param($ctx)
        $inv = [Globalization.CultureInfo]::InvariantCulture
        $rows = & $GetTrailRows $ctx.LogLines
        $bad = @()
        foreach ($r in $rows) {
          foreach ($c in @($r.X, $r.Y, $r.Z)) {
            $v = 0.0
            if (-not [double]::TryParse($c, [Globalization.NumberStyles]::Float, $inv, [ref]$v) -or [double]::IsNaN($v) -or [double]::IsInfinity($v)) { $bad += $r.Line; break }
          }
        }
        return @{ Pass = ($rows.Count -gt 0 -and $bad.Count -eq 0)
                  Detail = "$($rows.Count) row(s), $($bad.Count) with a non-finite spot$(if ($bad.Count) { '; first: ' + $bad[0] })" }
      }.GetNewClosure() }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
