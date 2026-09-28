# Real static-map queries at road speed, plus PCM after the Splicer's send gain.
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$case = & (Join-Path $PSScriptRoot 'FxEnclosureLive.ps1')
$case.Name = 'fx_static_passby'
$case.Run.MaxSeconds = 85
$case.DiagEnv += ',BRN_PASSBY_SOUND_DIAG=1'
$case.Checks += @(
  @{ Kind='LogCount'; Name='world static map posts a passby'; Pattern='\[sndenv\] static passby type='; Min=1 }
  @{ Kind='Script'; Name='passby sample reaches its destination after send gain'; Script={
      param($ctx)
      $trace = Join-Path $ctx.RunDir 'reverb_pcm_trace.txt'
      if (-not (Test-Path -LiteralPath $trace)) {
          if (-not (Test-Path -LiteralPath $ctx.Case.PcmTraceFile)) {
              return @{ Pass=$false; Detail='PCM trace missing' }
          }
          Copy-Item -LiteralPath $ctx.Case.PcmTraceFile -Destination $trace
      }
      $banks=@{}; $decoded=@{}; $count=0
      foreach($line in Get-Content -LiteralPath $trace) {
          if ($line -match '^splicer-bank bank=(\d+) name=\d+ passby=1$') {
              $banks[$Matches[1]]=$true
          } elseif ($line -match '^splicer-pcm .*sample=(\w+) voice=(\d+) .* rms=([^ ]+) peak=([^ ]+)$') {
              $rms=[double]::Parse($Matches[3],[Globalization.CultureInfo]::InvariantCulture)
              if ($banks.ContainsKey($Matches[2]) -and $rms -gt 0 -and -not [double]::IsInfinity($rms)) {
                  $decoded[$Matches[1]]=$true
              }
          } elseif ($line -match '^splicer-mix-pcm .*sample=(\w+) voice=(\d+) .* rms=([^ ]+) peak=([^ ]+)$') {
              $rms=[double]::Parse($Matches[3],[Globalization.CultureInfo]::InvariantCulture)
              if ($banks.ContainsKey($Matches[2]) -and $decoded.ContainsKey($Matches[1]) -and $rms -gt 0 -and -not [double]::IsInfinity($rms)) { $count++ }
          }
      }
      return @{ Pass=($count -gt 0); Detail="$count passby samples decoded and mixed at nonzero gain" }
  } }
)
$case
