# Cross the original surface-rumble route (surface4/16/2), through real wheel contacts.
$case = & (Join-Path $PSScriptRoot 'FxAemsPlaybackLive.ps1')
$case.Name = 'fx_road_surface_audio'
$case.Bug = 'Player tyre surface loops must select and decode the authored surface bank.'
$case.ProfileFixture = 'scratch/OWNERLIST_0927/L2/slot2_profile_drifted_21db22a8.sav'
$case.Run = @{
    Drive=$true; MotionProbe=$true; MaxSeconds=65; SkipIntro=$true; AcceptGap=1.0
    Teleport='2807.6,-3.0,-1474.7,26'
}
$case.Checks = @($case.Checks | Select-Object -First 3)
$case.Checks += @{
  Kind='Script'; Name='surface bank emits nonzero sample PCM'
  Script={
    param($ctx)
    $saved = Join-Path $ctx.RunDir 'aems_pcm_trace.txt'
    if (-not (Test-Path -LiteralPath $saved)) {
      if (-not (Test-Path -LiteralPath $ctx.Case.PcmTraceFile)) {
        return @{Pass=$false; Detail='PCM trace missing'}
      }
      Copy-Item -LiteralPath $ctx.Case.PcmTraceFile -Destination $saved
    }
    $banks=@{}; $samples=@{}; $count=0; $mixed=0
    foreach ($line in Get-Content -LiteralPath $saved) {
      if ($line -match '^aems-bank bank=(\d+) .*path=.*surface_patch_bank\.abi') {
        $banks[$Matches[1]]=$true
      } elseif ($line -match '^aems-select bank=(\d+) sample=(\w+) index=\d+$') {
        $samples[$Matches[2]]=$Matches[1]
      } elseif ($line -match '^aems-pcm .*sample=(\w+) .* peak=([^ ]+)$') {
        $sample=$Matches[1]
        $peak=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture)
        if ($samples.ContainsKey($sample) -and $banks.ContainsKey($samples[$sample]) -and $peak -gt 0) { $count++ }
      } elseif ($line -match '^aems-mix-pcm .*sample=(\w+) .* peak=([^ ]+)$') {
        $sample=$Matches[1]
        $peak=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture)
        if ($samples.ContainsKey($sample) -and $banks.ContainsKey($samples[$sample]) -and $peak -gt 0) { $mixed++ }
      }
    }
    @{Pass=($count -gt 0 -and $mixed -gt 0); Detail="$count nonzero surface sample decoders; $mixed nonzero send contributions"}
  }
}
$case
