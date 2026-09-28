# Land crashes with the console's random slow-motion scale; glass must reach the final Send.
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$case = & (Join-Path $root 'b5-decomp/tests/FxCrumpleAudioLive.ps1')
$case.Name = 'fx_crash_glass_audio'
$case.Bug = 'An authored glass smash must reach the collision mixer.'
$case.Run.Audio = $true
$case.Run.MaxSeconds = 105
$case.Run.CrashSweepShots = '220:53,225:70,230:70'
$case.Run.CrashSweepSettle = 600
$case.DiagEnv = $case.DiagEnv.Replace(',BRN_ULTRA_SLOMO_SCALE=0.0075','')
$case.Checks = @($case.Checks[0..4]) + @($case.Checks[8..9])
$case.Checks += @{
  Kind='Script'; Name='original glass sample contributes audible PCM'; Script={
    param($ctx)
    $dest=Join-Path $ctx.RunDir 'glass_pcm_trace.txt'
    if (-not (Test-Path -LiteralPath $dest)) { Copy-Item -LiteralPath $ctx.Case.PcmTraceFile -Destination $dest }
    $events=@{}; $mixed=0; $ids=@{}
    foreach ($line in Get-Content -LiteralPath $dest) {
      if ($line -match '^splice-output-source sample=(\w+) bank=\d+ event=(\d+) spec=(\d+)') {
        $sample=$Matches[1];$event=[int]$Matches[2]
        if ($Matches[3] -eq '2036857685' -and $event -ge 251 -and $event -le 263) { $events[$sample]=$event }
        else { $events.Remove($sample) }
      } elseif ($line -match '^splice-output-pcm .*sample=(\w+) .* peak=([^ ]+)$') {
        $sample=$Matches[1]
        $peak=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture)
        if ($events.ContainsKey($sample) -and $peak -gt 0 -and -not [double]::IsInfinity($peak)) { $mixed++;$ids[$events[$sample]]=$true }
      }
    }
    @{Pass=($mixed -gt 0);Detail="glass Send=$mixed sample IDs=$($ids.Keys -join ',')"}
  }
}
$case
