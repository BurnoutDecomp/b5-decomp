# Original environment scenario with actual preset, voice, and DSP output witnesses.
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$case = & (Join-Path $root 'tools/tests/cases/audio_environment.ps1')
$case.Name = 'fx_reverb'
$case.ProfileFixture = 'rival_hunt_profile.sav'
$trace = 'reverb_pcm_' + [Guid]::NewGuid().ToString('N') + '.txt'
$case.PcmTraceFile = Join-Path $root ('build/game/' + $trace)
$case.DiagEnv += ',BRN_SOUND_PCM_TRACE=' + $trace
$case.Checks += @(
  @{ Kind='LogCount'; Name='authored reverb preset loaded after fade'; Pattern='\[sndenv\] reverb preset=\d+ time='; Min=1 }
  @{ Kind='LogCount'; Name='reverb parameters and gain reached live voice'; Pattern='\[sndenv\] reverb applied ready=1'; Min=1 }
  @{ Kind='Script'; Name='reverb DSP emits finite nonzero PCM'; Script={
    param($ctx)
    $saved = Join-Path $ctx.RunDir 'reverb_pcm_trace.txt'
    if (-not (Test-Path -LiteralPath $saved)) {
      if (-not (Test-Path -LiteralPath $ctx.Case.PcmTraceFile)) {
        return @{ Pass=$false; Detail='PCM trace missing' }
      }
      Copy-Item -LiteralPath $ctx.Case.PcmTraceFile -Destination $saved
    }
    $count = 0
    foreach ($line in Get-Content -LiteralPath $saved) {
      if ($line -match '^effect-pcm name=reverb .* rms=([^ ]+) peak=([^ ]+)$') {
        $rms = [double]::Parse($Matches[1], [Globalization.CultureInfo]::InvariantCulture)
        $peak = [double]::Parse($Matches[2], [Globalization.CultureInfo]::InvariantCulture)
        if ($rms -gt 0 -and $peak -gt 0 -and -not [double]::IsInfinity($rms) -and -not [double]::IsInfinity($peak)) { $count++ }
      }
    }
    return @{ Pass=($count -gt 0); Detail="$count reverb processors emitted finite nonzero PCM" }
  }}
)
$case
