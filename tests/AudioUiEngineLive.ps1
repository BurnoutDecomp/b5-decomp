# Boot, operate GUI, accelerate and lift through the ordinary input chain.
# Capture mixer witnesses and actual engine-send PCM, with the device muted.
@{
  Name = 'audio_ui_engine'
  Area = 'audio'
  Bug = 'GUI routing, live camera attenuation and below-minimum Ginsu pitch must remain correct'
  Frames = $false
  Run = @{ MaxSeconds=75; SkipIntro=$true; Drive=$true; ThrottleScript='0:accel,12:none,18:accel' }
  DiagEnv = 'BRN_ENGINE_GAIN_DIAG=1,BRN_HUD_SOUND_DIAG=1,BRN_SOUND_PCM_TRACE=audio_ui_engine_pcm.txt'
  Setup = {
    param($ctx)
    # Slot executables still use build/game as their working directory. Give
    # fopen an absolute, run-owned path rather than guessing from the log path.
    $ctx.Case.PcmTraceFile = Join-Path $ctx.RunDir 'audio_ui_engine_pcm.live.txt'
    if ($ctx.Case.PcmTraceFile -match '[,\s=]') {
      throw 'The flow harness requires a PCM trace path without whitespace, commas or equals signs.'
    }
    if (Test-Path -LiteralPath $ctx.Case.PcmTraceFile) {
      throw 'Preserve the previous run: use a new RunDir for the PCM trace.'
    }
    $ctx.Case.DiagEnv = $ctx.Case.DiagEnv -replace 'BRN_SOUND_PCM_TRACE=[^,\s]+', ('BRN_SOUND_PCM_TRACE='+$ctx.Case.PcmTraceFile)
  }
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
      # run_case has no PcmTraceFile hook. Archive this run's trace here so
      # -NoRun can re-evaluate it without consulting a later game's output.
      if (-not (Test-Path -LiteralPath $trace)) {
        $source=Get-Item -LiteralPath $ctx.Case.PcmTraceFile -ErrorAction SilentlyContinue
        $started=$null; $duration=$null
        if ($ctx.MarksText -match '(?m)^RUNSTART (.+)\r?$') {
          $started=[datetimeoffset]::Parse($Matches[1], [Globalization.CultureInfo]::InvariantCulture).UtcDateTime
        }
        # flow_run formats elapsed times with the host culture (e.g. 75,2s).
        if ($ctx.MarksText -match '(?m)^END\s+([\d.,]+)s') {
          $duration=[double]::Parse($Matches[1].Replace(',','.'), [Globalization.CultureInfo]::InvariantCulture)
        }
        if ($source -and $started -and $null -ne $duration -and
            $source.LastWriteTimeUtc -ge $started -and
            $source.LastWriteTimeUtc -le $started.AddSeconds($duration + 5)) {
          Copy-Item -LiteralPath $source.FullName -Destination $trace
        }
      }
      if (-not (Test-Path -LiteralPath $trace)) {
        return @{ Pass=$false; Detail='PCM trace for this run is missing or stale' }
      }
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
