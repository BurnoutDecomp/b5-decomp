# Water impact and reset stings must survive their final voice gain/routing.
$case = & (Join-Path $PSScriptRoot 'FxWaterRecoveryLive.ps1')
$trace = & (Join-Path $PSScriptRoot 'FxAemsPlaybackLive.ps1')
$case.Name = 'fx_crash_water_audio'
$case.Bug = 'Water entry and road recovery must play their authored stings through the final Send.'
$case.PcmTraceFile = $trace.PcmTraceFile
$case.DiagEnv += ',BRN_SOUND_PCM_TRACE=' + (Split-Path $case.PcmTraceFile -Leaf)
$reset = & (Join-Path $PSScriptRoot 'FxCrashSndResetStingLive.ps1')
$case.Checks += $reset.Checks[2..6]
$case.Checks += @{
  Kind='Script'; Name='water and reset stings reach their final destination'
  Script={
    param($ctx)
    $saved = Join-Path $ctx.RunDir 'crash_water_pcm_trace.txt'
    Copy-Item -LiteralPath $ctx.Case.PcmTraceFile -Destination $saved
    $events=@{}; $water=0; $reset=0
    foreach ($line in Get-Content -LiteralPath $saved) {
      # Notify case 8 uses CollisionSpliceBank; reset uses FX_splice.
      if ($line -match '^splice-output-source sample=(\w+) bank=\d+ event=(\d+) spec=(\d+)$') { $events[$Matches[1]]=$Matches[3]+':'+$Matches[2] }
      elseif ($line -match '^splice-output-pcm .*sample=(\w+) .* rms=([^ ]+) peak=([^ ]+)$') {
        $sample=$Matches[1]; $rms=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture)
        if ($rms -gt 0 -and -not [double]::IsInfinity($rms) -and $events.ContainsKey($sample)) {
          if ($events[$sample] -eq '2036857685:337') { $water++ }
          if ($events[$sample] -eq '1188873853:1') { $reset++ }
        }
      }
    }
    @{Pass=($water -gt 0 -and $reset -gt 0); Detail="nonzero final Send contributions: water=$water reset=$reset"}
  }
}
$case
