# Verify real Send output during the original 70 m/s wall impact-time shot.
$case = & (Join-Path $PSScriptRoot 'FxCrumpleAudioLive.ps1')
$case.Name = 'fx_crash_audio_mix'
$case.Bug = 'Collision, body-part, scrape, crumple and crash-stream layers must reach their destination mixers.'
$case.Run.Audio = $true
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$case.AudioCapture = Join-Path $root ('build/game/crash_audio_' + [Guid]::NewGuid().ToString('N') + '.wav')
$case.DiagEnv += ',BRN_AUDIO_CAPTURE=' + $case.AudioCapture
$deform = & (Join-Path $PSScriptRoot 'FxCrashSnd2DeformLive.ps1')
$scrape = & (Join-Path $PSScriptRoot 'FxCrashSnd2ScrapesLive.ps1')
$case.Checks += $deform.Checks[2..9]
$case.Checks += $scrape.Checks[2..6]
$case.Checks += @{
  Kind='Script'; Name='crash layers contribute real PCM after their volume controls'
  Script={
    param($ctx)
    $trace = Join-Path $ctx.RunDir 'crumple_pcm_trace.txt'
    $banks=@{}; $samples=@{}; $waves=@{}; $events=@{}; $body=@{}
    $impact=0; $parts=0; $scrapes=0; $crumple=0; $streams=0
    foreach ($line in $ctx.LogLines) {
      if ($line -match '^\[collision-audio\] play pipeline=0 bin=\d+ size=\d+ sample=(\d+) .* action=(2|4|8|16) ') { $body[$Matches[1]]=$true }
    }
    foreach ($line in Get-Content -LiteralPath $trace) {
      if ($line -match '^aems-bank bank=(\d+) .*path=.*(CrumplePatchBank|ScrapePatchBank)\.abi') { $banks[$Matches[1]]=$Matches[2] }
      elseif ($line -match '^aems-select bank=(\d+) sample=(\w+) ') { $samples[$Matches[2]]=$Matches[1] }
      elseif ($line -match '^wave-source .*sample=(\w+) .*path=(.*)') { $waves[$Matches[1]]=$Matches[2] }
      elseif ($line -match '^splice-output-source sample=(\w+) bank=\d+ event=(\d+) spec=(\d+)') {
        # Playback::Name::MakeHash("CollisionSpliceBank") = 2036857685.
        if ($Matches[3] -eq '2036857685') { $events[$Matches[1]]=$Matches[2] }
        else { $events.Remove($Matches[1]) }
      }
      elseif ($line -match '^splice-output-pcm .*sample=(\w+) .* peak=([^ ]+)$') {
        $sample=$Matches[1]
        if ($events.ContainsKey($sample)) { $impact++; if ($body.ContainsKey($events[$sample])) { $parts++ } }
      }
      elseif ($line -match '^aems-mix-pcm .*sample=(\w+) .* peak=([^ ]+)$') {
        $sample=$Matches[1]
        if ($samples.ContainsKey($sample) -and $banks.ContainsKey($samples[$sample])) {
          if ($banks[$samples[$sample]] -eq 'CrumplePatchBank') { $crumple++ } else { $scrapes++ }
        }
      }
      elseif ($line -match '^wave-mix-pcm .*sample=(\w+) .* peak=([^ ]+)$') {
        if ($waves.ContainsKey($Matches[1]) -and $waves[$Matches[1]] -match '^CRASH\d+\.SNS$') { $streams++ }
      }
    }
    @{Pass=($impact -gt 0 -and $parts -gt 0 -and $scrapes -gt 0 -and $crumple -gt 0 -and $streams -gt 0);
      Detail="nonzero Send contributions: collisions=$impact body-parts=$parts scrapes=$scrapes crumple=$crumple crash-stream=$streams"}
  }
}
$case.Checks += @{
  Kind='Script'; Name='audible host output is captured'
  Script={
    param($ctx)
    $dest=Join-Path $ctx.RunDir 'host_output.wav'
    if (-not (Test-Path -LiteralPath $ctx.Case.AudioCapture)) { return @{Pass=$false;Detail='No host capture'} }
    Copy-Item -LiteralPath $ctx.Case.AudioCapture -Destination $dest
    @{Pass=((Get-Item -LiteralPath $dest).Length -gt 44);Detail='Host PCM capture saved with the run'}
  }
}
$case
