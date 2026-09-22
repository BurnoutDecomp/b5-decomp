# Direct coverage of the restored Showtime-to-traffic shaping branch.
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'car_car_bounce'
$case.Bug = 'Showtime traffic impacts must execute the original mass-shaped bounce response.'
$case.DiagEnv += ',BRN_TRAFFIC_DIAG=1'
$case.Checks = @($case.Checks | Where-Object { $_.Name -ne 'Showtime world contact response ran' })
$case.Checks += @{
    Kind = 'LogCount'; Name = 'Showtime traffic impulse shaping ran'
    Pattern = '\[showtime-car-contact\] other='; Min = 1
}
$case.Checks += @{
    Kind = 'LogCount'; Name = 'both car-car impulse halves completed'
    Pattern = '\[car-car-contact\] pair-applied other='; Min = 1
}
$case
