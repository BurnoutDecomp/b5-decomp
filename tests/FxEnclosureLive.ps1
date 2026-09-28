# Drive the I-88 tunnel using real region queries; require authored tunnel preset.
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$case = & (Join-Path $PSScriptRoot 'FxReverbLive.ps1')
$case.Name = 'fx_enclosure'
$case.Run = @{
  Drive=$true; MaxSeconds=75; SkipIntro=$true; AcceptGap=1.0
  Teleport='3013.0,-7.5,-1150.0,0'; ThrottleScript='0:accel'
}
$case.Checks = @($case.Checks | Where-Object {
  $_.Name -in @('no NEW assert families','no exceptions','reached DRIVING',
               'no assert from the environment sound files','reverb DSP emits finite nonzero PCM')
})
$case.Checks += @(
  @{ Kind='LogCount'; Name='real tunnel query reaches enclosure control'; Pattern='\[sndenv\] enclosure active=\d+ previous=\d+ tunnel=1'; Min=1 }
  @{ Kind='LogCount'; Name='tunnel reverb preset loaded'; Pattern='\[sndenv\] reverb preset=1 time='; Min=1 }
  @{ Kind='LogCount'; Name='info: enclosure whooshes'; Pattern='\[sndenv\] enclosure passby type='; Min=0 }
)
$case
