$case = & (Join-Path $PSScriptRoot 'CrashActions.ps1')
$case.Name = 'traffic_crash_lifecycle'
$case.Bug = 'Live traffic crashes must enter owner tracking and clear records when traffic removes their bodies.'
$case.Checks += @(
    @{ Kind = 'LogCount'; Name = 'traffic crash recorded'; Pattern = '\[traffic-crash\] added vehicle='; Min = 1 }
    @{ Kind = 'Script'; Name = 'recorded traffic crash is removed'; Script = {
        param($ctx)
        $tracked = @{}
        $removed = 0
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[traffic-crash\] added vehicle=(\d+) owner=(\d+)') { $tracked[$Matches[1]] = $Matches[2] }
            if ($line -match '\[traffic-crash\] (?:removed|cleanup) vehicle=(\d+) owner=(\d+)') {
                if ($tracked.ContainsKey($Matches[1]) -and $tracked[$Matches[1]] -eq $Matches[2]) {
                    ++$removed
                    $tracked.Remove($Matches[1])
                }
            }
        }
        @{ Pass = $removed -gt 0; Detail = "$removed recorded crashes removed with the same owner" }
    } }
)
$case
