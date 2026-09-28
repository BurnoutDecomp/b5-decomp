$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$case = & (Join-Path $root 'b5-decomp/tests/FxTailsBPassbyLive.ps1')
$pcmCase = & (Join-Path $root 'b5-decomp/tests/FxStaticPassbyLive.ps1')
$case.ProfileFixture = 'scratch/OWNERLIST_0927/L2/slot2_profile_drifted_21db22a8.sav'
$case.PcmTraceFile = $pcmCase.PcmTraceFile
$case.DiagEnv += ',BRN_SOUND_PCM_TRACE=' + (Split-Path $case.PcmTraceFile -Leaf)
$case.Checks += $pcmCase.Checks[-1]
$case.Run.Audio = $true
$case.AudioCapture = Join-Path $root ('build/game/owner_sound_' + [Guid]::NewGuid().ToString('N') + '.wav')
$case.DiagEnv += ',BRN_AUDIO_CAPTURE=' + $case.AudioCapture + ',BRN_ENGINE_MIX_DIAG=1,BRN_SOUNDLOGIC_DIAG=1'
$case.Checks += @(@{ Kind='Script'; Name='host output capture available'; Script={
  param($ctx)
  if (-not (Test-Path -LiteralPath $ctx.Case.AudioCapture)) {
    return @{ Pass=$false; Detail='Host PCM capture absent' }
  }
  $target = Join-Path $ctx.RunDir 'host_output.wav'
  Copy-Item -LiteralPath $ctx.Case.AudioCapture -Destination $target
  @{ Pass=((Get-Item -LiteralPath $target).Length -gt 44); Detail='Host PCM captured for waveform analysis' }
}})

$fade = & (Join-Path $root 'b5-decomp/tests/FxTailsBStreamFadeLive.ps1')
$case.Name = 'fx_audio_streams'
$case.DiagEnv += ',BRN_STREAM_DIAG=1,BRN_MUSIC_DIAG=1,BRN_SPEECH_DIAG=1'
$case.Checks += $fade.Checks[1..3]
$case.Checks += @(@{ Kind='Script'; Name='streamed wave content emits nonzero PCM'; Script={
  param($ctx)
  $trace = Join-Path $ctx.RunDir 'reverb_pcm_trace.txt'
  $n=0
  foreach($l in Get-Content -LiteralPath $trace) {
    if ($l -match '^wave-pcm .*rms=([^ ]+) peak=([^ ]+)$') {
      $r=[double]::Parse($Matches[1],[Globalization.CultureInfo]::InvariantCulture)
      if ($r -gt 0 -and -not [double]::IsInfinity($r)) {$n++}
    }
  }
  @{Pass=($n -gt 0);Detail="$n wave players emit nonzero decoded PCM; attribution in trace"}
}})
$case.Bug = 'Engine synthesis and authored streamed sounds must emit decoded PCM and reach the final voice send.'
$case.Checks += @(@{ Kind='Script'; Name='engine synthesis reaches its destination'; Script={
  param($ctx)
  $n=@(Get-Content -LiteralPath (Join-Path $ctx.RunDir 'reverb_pcm_trace.txt') | Where-Object { $_ -match '^ginsu-mix-pcm .*rms=([^ ]+) peak=([^ ]+)$' }).Count
  @{Pass=($n -gt 0);Detail="$n engine send contributions contain real PCM"}
}})
$case
