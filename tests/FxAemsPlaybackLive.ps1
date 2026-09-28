# Requires original sound data converted by build data --only SOUND/AEMS/*.
# The proof follows a traffic bank sample into the decoder's actual PCM buffer.
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$case = & (Join-Path $root 'tools/tests/cases/audio_traffic.ps1')
$case.Name = 'fx_aems_playback'
$case.ProfileFixture = 'rival_hunt_profile.sav'
$case.Run.MaxSeconds = 55
$trace = 'aems_pcm_' + [Guid]::NewGuid().ToString('N') + '.txt'
$case.PcmTraceFile = Join-Path $root ('build/game/' + $trace)
$case.DiagEnv += ',BRN_SOUND_PCM_TRACE=' + $trace
$case.Checks += @{
  Kind = 'Script'; Name = 'traffic bank emits nonzero decoded PCM'
  Script = {
    param($ctx)
    $saved = Join-Path $ctx.RunDir 'aems_pcm_trace.txt'
    if (-not (Test-Path -LiteralPath $saved)) {
      if (-not (Test-Path -LiteralPath $ctx.Case.PcmTraceFile)) {
        return @{ Pass = $false; Detail = 'PCM trace missing' }
      }
      Copy-Item -LiteralPath $ctx.Case.PcmTraceFile -Destination $saved
    }
    $banks = @{}
    $samples = @{}
    $count = 0
    foreach ($line in Get-Content -LiteralPath $saved) {
      if ($line -match '^aems-class bank=(\d+) kind=1 name=TrafficEngineClass$') {
        $banks[$Matches[1]] = $true
      } elseif ($line -match '^aems-select bank=(\d+) sample=(\w+) index=\d+$') {
        $samples[$Matches[2]] = $Matches[1]
      } elseif ($line -match '^aems-pcm .*sample=(\w+) .* rms=([^ ]+) peak=([^ ]+)$') {
        $sample = $Matches[1]
        $peak = [double]::Parse($Matches[3], [Globalization.CultureInfo]::InvariantCulture)
        if ($samples.ContainsKey($sample) -and $banks.ContainsKey($samples[$sample]) -and $peak -gt 0) { $count++ }
      }
    }
    return @{ Pass = ($count -gt 0); Detail = "$count traffic sample players emitted nonzero PCM" }
  }
}
$case
