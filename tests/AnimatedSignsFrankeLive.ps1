# AI.DAT section 2807 portals belong to Franke road 17 via STREETDATA.DAT span 72.
$case = & (Join-Path $PSScriptRoot 'AnimatedSignsLive.ps1')
$case.Name = 'animated_signs_franke'
$case.Run.Teleport = '2060,13,-521.8,210'
$case.Run.MaxSeconds = 65
$case.Checks += @{ Kind='LogMatch'; Name='car actually seats on Franke Avenue'; Pattern='\[teleport\] ProcessResetEvents car 0 road \(2060\.' }
$case
