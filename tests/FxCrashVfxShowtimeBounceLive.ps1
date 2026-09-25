# FX-CRASHVFX (crash parity 2026-09-25, item 5) -- a Showtime bounce on a car BURSTS THE WRECK, live.
#
# EffectsModule::HandleShowtimeTrafficBounce @0x82292808 takes every JUST_BOUNCED (144) record HandleGameActions hands
# it. A bounce that lands on a car with a good impact fires, at most once every 0.5 s: the player's debris
# (FireDebrisBurst), a pane of glass debris (BurstAreaEmitParticles), a spark shower (DoSparkShower) and the
# 'ExploShort' LION effect. Before the fix the handler announced itself NOT RECONSTRUCTED and a bounce showed none of it.
#
# The drive is FxShowtime2Live's: ShowtimeContacts.ps1's Showtime, with the real boost button pulsed so the wreck
# bounces along the road into traffic. The witnesses (NOT IN THE X360 BINARY, default off):
#   [bounce-vfx]      BRN_BOUNCE_VFX_DIAG=1 -- one line per record handled: the time and the last one, the record's
#                     mbOnCar / mbGoodImpact, and the verdict (too-soon / no-vehicle-impact / fired + the LION slot);
#   [showtime-bounce] BRN_SHOWTIME_WATCH=1 (ShowtimeContacts') -- one line per bounce the game state relays as 144;
#   [debris-burst]    BRN_DEBRIS_BURST_DIAG=1 -- the burst record the particle module consumed (t, scale, pieces).
# NO FRAME DUMP.
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'fxcrashvfx_showtime_bounce'
$case.Area = 'vfx'
$case.Bug = 'A Showtime bounce on a car must burst the wreck where it hit: the car''s debris, glass, a spark shower and the ExploShort explosion, at most once every half second.'
$case.DiagEnv += ',BRN_BOUNCE_VFX_DIAG=1,BRN_DEBRIS_BURST_DIAG=1'
$case.Run.Boost = '50'
$case.Run.MaxSeconds = 179

$GetBounceRows = {
  param($lines)
  $inv = [Globalization.CultureInfo]::InvariantCulture
  $rows = @(); $n = 0
  foreach ($line in $lines) {
    $n++
    if ($line -notmatch '^\[bounce-vfx\] t=(?<t>\S+) last=(?<last>\S+) onCar=(?<car>-?\d+) good=(?<good>-?\d+) playing=(?<play>\d) -> (?<verdict>too-soon|no-vehicle-impact|fired)(?<rest>.*)$') { continue }
    $t = 0.0; $last = 0.0
    [void][double]::TryParse($Matches.t, [Globalization.NumberStyles]::Float, $inv, [ref]$t)
    [void][double]::TryParse($Matches.last, [Globalization.NumberStyles]::Float, $inv, [ref]$last)
    $rows += [pscustomobject]@{ Index = $n; T = $t; TText = $Matches.t; Last = $last; LastText = $Matches.last
      OnCar = [int]$Matches.car; Good = [int]$Matches.good; Playing = [int]$Matches.play; Verdict = $Matches.verdict
      Rest = $Matches.rest; Line = $line }
  }
  return ,$rows
}

$case.Checks += @(
  @{ Kind = 'LogCount'; Name = 'the bounce handler runs: no NOT RECONSTRUCTED announcement'
     Pattern = 'NOT RECONSTRUCTED: EffectsModule::HandleShowtimeTrafficBounce'; Max = 0 }
  @{ Kind = 'Script'; Name = 'every bounce the game state relays (144) reaches the handler'; Script = {
      param($ctx)
      $relayed = @($ctx.LogLines | Where-Object { $_ -match '^\[showtime-bounce\] event 52 -> action 144' }).Count
      $rows = & $GetBounceRows $ctx.LogLines
      # [showtime-bounce] is capped at 64 lines, [bounce-vfx] at 128: compare below the caps.
      $pass = ($relayed -gt 0) -and (($rows.Count -eq $relayed) -or ($relayed -ge 64 -and $rows.Count -ge 64))
      @{ Pass = $pass; Detail = "$relayed relayed, $($rows.Count) handled" }
    }.GetNewClosure() }
  @{ Kind = 'Script'; Name = 'each verdict is the console''s gate: fired only on a car with a good impact and 0.5 s after the last'; Script = {
      param($ctx)
      $rows = & $GetBounceRows $ctx.LogLines
      $bad = @(); $counts = @{ 'too-soon' = 0; 'no-vehicle-impact' = 0; 'fired' = 0 }
      foreach ($r in $rows) {
        $counts[$r.Verdict]++
        # `fcmpu ; blt` on |t - last| against 0.5 first, then playback or (mbOnCar && mbGoodImpact). The witness prints
        # both times to 3 decimals, so a gap within 0.001 of 0.5 is not judged.
        $gap = [math]::Abs($r.T - $r.Last)
        if ([math]::Abs($gap - 0.5) -lt 0.0015) { continue }
        $expect = if ($gap -lt 0.5) { 'too-soon' } elseif ($r.Playing -eq 1 -or ($r.OnCar -eq 1 -and $r.Good -ne 0)) { 'fired' } else { 'no-vehicle-impact' }
        if ($r.Verdict -ne $expect) { $bad += "$($r.Line) (expected $expect)" }
      }
      @{ Pass = ($rows.Count -gt 0 -and $bad.Count -eq 0)
         Detail = "$($rows.Count) handled: $($counts['fired']) fired, $($counts['too-soon']) too soon, $($counts['no-vehicle-impact']) not on a car / no good impact; $($bad.Count) off the gate$(if ($bad.Count) { '; first: ' + $bad[0] })" }
    }.GetNewClosure() }
  @{ Kind = 'Script'; Name = 'a bounce on a car FIRED and its ExploShort LION effect resolved'; Script = {
      param($ctx)
      $rows = & $GetBounceRows $ctx.LogLines
      $fired = @($rows | Where-Object { $_.Verdict -eq 'fired' })
      $resolved = @($fired | Where-Object { $_.Rest -match ' resolved$' })
      $first = if ($fired.Count) { $fired[0].Line } else { '' }
      @{ Pass = ($resolved.Count -gt 0); Detail = "$($fired.Count) fired, $($resolved.Count) with the effect resolved; first: $first" }
    }.GetNewClosure() }
  @{ Kind = 'Script'; Name = 'the fired bounce''s debris burst was consumed by the particle module ([debris-burst] at its time, scale 1)'; Script = {
      param($ctx)
      $rows = & $GetBounceRows $ctx.LogLines
      $fired = @($rows | Where-Object { $_.Verdict -eq 'fired' })
      if ($fired.Count -eq 0) { return @{ Pass = $false; Detail = 'no bounce fired' } }
      $bursts = @(); $n = 0
      foreach ($l in $ctx.LogLines) { $n++; if ($l -match '^\[debris-burst\] t=(\S+) scale=(\S+) pieces=(\S+)') { $bursts += [pscustomobject]@{ Index = $n; T = $Matches[1]; Scale = $Matches[2]; Pieces = $Matches[3] } } }
      $matched = @(); $budgetGone = 0
      foreach ($f in $fired) {
        $hit = @($bursts | Where-Object { $_.T -eq $f.TText -and $_.Scale -eq '1.000' -and $_.Index -gt $f.Index })
        if ($hit.Count) { $matched += "t=$($f.TText) pieces=$($hit[0].Pieces)" }
        elseif (@($bursts | Where-Object { $_.Index -lt $f.Index }).Count -ge 40) { $budgetGone++ }
      }
      # [debris-burst] is capped at 40 lines: a fired bounce after the budget ran out cannot show its burst.
      @{ Pass = ($matched.Count -gt 0 -or $budgetGone -eq $fired.Count)
         Detail = "$($fired.Count) fired, $($matched.Count) matched ($(($matched | Select-Object -First 4) -join '; ')), $budgetGone after the 40-line budget" }
    }.GetNewClosure() }
  @{ Kind = 'LogCount'; Name = 'the handler''s own asserts stay quiet (player active and crashing; the ExploShort description found)'
     Pattern = 'IsPlayerCarCrashing\(\)|Couldn''t locate lion effect description .*ExploShort'; Max = 0 }
)
$case
