# Boot, operate GUI, accelerate and lift through the ordinary input chain.
# Capture mixer witnesses and actual engine-send PCM, with the device muted.
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
@{
  Name = 'audio_ui_engine'
  Area = 'audio'
  Bug = 'GUI routing, live camera attenuation and below-minimum Ginsu pitch must remain correct'
  Frames = $false
  Run = @{ MaxSeconds=75; SkipIntro=$true; Drive=$true; ThrottleScript='0:accel,12:none,18:accel' }
  PcmTraceFile = Join-Path $root 'build/game/audio_ui_engine_pcm.txt'
  DiagEnv = 'BRN_ENGINE_GAIN_DIAG=1,BRN_HUD_SOUND_DIAG=1,BRN_SOUND_PCM_TRACE=audio_ui_engine_pcm.txt'
  Checks = @(
    @{ Kind='NewAsserts'; Name='no new assertion families' }
    @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
    @{ Kind='Mark'; Name='reached driving'; Phase='DRIVING' }
    @{ Kind='Script'; Name='engine synth frequency respects the authored minimum'; Script={
      param($ctx)
      $count=0; $below=0; $audible=0
      foreach ($line in $ctx.LogLines) {
        if ($line -match '\[enggain\] rpm ([\d.]+) minRpm ([\d.]+).* gain ([\d.]+)') {
          $count++
          if ([double]$Matches[1] + 0.01 -lt [double]$Matches[2]) { $below++ }
          if ([double]$Matches[3] -gt 0) { $audible++ }
        }
      }
      @{ Pass=($count -gt 10 -and $below -eq 0 -and $audible -gt 0);
         Detail="$count engine observations, $below below minimum, $audible with nonzero final gain" }
    } }
    @{ Kind='Script'; Name='GUI button sounds use the volume output'; Script={
      param($ctx)
      $count=0; $bad=0
      foreach ($line in $ctx.LogLines) {
        if ($line -match '\[hud-sound-msg\] type=2 .* -> PLAY .*mixer=(\d+)') {
          $count++
          if ([int]$Matches[1] -ne 3) { $bad++ }
        }
      }
      @{ Pass=($count -gt 0 -and $bad -eq 0); Detail="$count button sounds, $bad incorrect mixer slots" }
    } }
    @{ Kind='Script'; Name='engine send contributes nonzero PCM'; Script={
      param($ctx)
      $trace=Join-Path $ctx.RunDir 'reverb_pcm_trace.txt'
      $count=0
      if (Test-Path -LiteralPath $trace) {
        foreach ($line in Get-Content -LiteralPath $trace) {
          if ($line -match '^ginsu-mix-pcm .*rms=([^ ]+)') {
            $value=[double]::Parse($Matches[1], [Globalization.CultureInfo]::InvariantCulture)
            if ($value -gt 0 -and -not [double]::IsInfinity($value)) { $count++ }
          }
        }
      }
      @{ Pass=($count -gt 0); Detail="$count engine sends produced nonzero PCM" }
    } }
  )
}
