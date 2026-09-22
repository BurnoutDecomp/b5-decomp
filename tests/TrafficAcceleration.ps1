$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'traffic_acceleration'
$case.Bug = 'Traffic must use the original recovery acceleration and Showtime proximity speed cap.'
$case.DiagEnv += ',BRN_TRAFFIC_DIAG=1'
$case.Checks += @{
    Kind = 'LogCount'; Name = 'traffic uses Showtime proximity speed cap'
    Pattern = '\[traffic-accel\] showtime param='; Min = 1
}
$case.Checks += @{
    Kind = 'LogCount'; Name = 'traffic uses slam or swerve recovery acceleration'
    Pattern = '\[traffic-accel\] recovery param='; Min = 1
}
$case
