# The original maximum command on a moving player car, followed by settling.
# Reuses the frozen-case checks but requires active suspension at the callback.
# Returning fixture and private slot restoration are handled by run_case.ps1.
$case = & (Join-Path $PSScriptRoot 'PlaytestMaxDeformationLive.ps1')
$case.Name = 'playtest_max_deformation_active'
$case.Bug = 'Trace maximum deformation through active body pose, wheel contacts and suspension.'
$case.Run.Drive = $true
$case.Run.DriveDelay = 4.0
$case.Run.DriveSeconds = 4.0
$case.Run.PauseAt = '20'
$case.Run.UnpauseAt = '27'
$case.DiagEnv += ',BRN_WHEEL_SUS_PROBE=1'
$case.Checks += @(
    @{ Kind='Script'; Name='maximum command reaches an active player suspension step'; Script={
        param($ctx)
        $callback = -1
        $wheelWorld = @()
        for ($i=0; $i -lt $ctx.LogLines.Count; ++$i) {
            $line = $ctx.LogLines[$i]
            if ($line -match '^\[max-deform\] phase callback ') { $callback = $i }
            if ($line -match '^\[max-deform-wheel\] sample 0 .* world (?<x>\S+) (?<y>\S+) (?<z>\S+)') {
                $wheelWorld += ,@([double]::Parse($Matches.x,[cultureinfo]::InvariantCulture),
                    [double]::Parse($Matches.y,[cultureinfo]::InvariantCulture),
                    [double]::Parse($Matches.z,[cultureinfo]::InvariantCulture))
            }
        }
        if ($callback -lt 0 -or $wheelWorld.Count -ne 4) {
            return @{ Pass=$false; Detail="callback=$callback, wheel centres=$($wheelWorld.Count)" }
        }
        $centre = @(0.0,0.0,0.0)
        foreach ($wheel in $wheelWorld) { for ($axis=0; $axis -lt 3; ++$axis) { $centre[$axis] += $wheel[$axis]*0.25 } }
        # The player is selected by the actual maximum wheel centres, not by a
        # guessed pointer or driver-type zero (parked traffic can share that type).
        $nearest = $null
        for ($i=[math]::Max(0,$callback-2000); $i -lt $callback; ++$i) {
            if ($ctx.LogLines[$i] -notmatch '^\[wsus\] f (?<f>\d+) car (?<car>\d+) pos (?<x>\S+) (?<y>\S+) (?<z>\S+) vel (?<vx>\S+) (?<vy>\S+) (?<vz>\S+)') { continue }
            $pos = @([double]::Parse($Matches.x,[cultureinfo]::InvariantCulture),
                [double]::Parse($Matches.y,[cultureinfo]::InvariantCulture),
                [double]::Parse($Matches.z,[cultureinfo]::InvariantCulture))
            $vel = @([double]::Parse($Matches.vx,[cultureinfo]::InvariantCulture),
                [double]::Parse($Matches.vy,[cultureinfo]::InvariantCulture),
                [double]::Parse($Matches.vz,[cultureinfo]::InvariantCulture))
            $distance2=0.0; $speed2=0.0
            for ($axis=0; $axis -lt 3; ++$axis) {
                $distance2 += [math]::Pow($pos[$axis]-$centre[$axis],2)
                $speed2 += $vel[$axis]*$vel[$axis]
            }
            if (-not $nearest -or $distance2 -lt $nearest.Distance2) {
                $nearest = @{ Car=$Matches.car; Frame=$Matches.f; Distance2=$distance2; Speed2=$speed2 }
            }
        }
        if (-not $nearest) { return @{ Pass=$false; Detail='no suspension witness before the maximum callback' } }
        $contacts = @()
        foreach ($line in $ctx.LogLines) {
            if ($line -match '^\[wsus-w\] f (?<f>\d+) car (?<car>\d+) w (?<w>\d+) onG (?<g>[01]) ') {
                if ($Matches.car -eq $nearest.Car -and $Matches.f -eq $nearest.Frame) { $contacts += "$($Matches.w):$($Matches.g)" }
            }
        }
        $grounded = @($contacts | Where-Object { $_.EndsWith(':1') }).Count
        @{ Pass=($nearest.Distance2 -lt 9.0 -and $nearest.Speed2 -gt 0.01 -and $contacts.Count -eq 4 -and $grounded -ge 2)
           Detail="player car=$($nearest.Car), frame=$($nearest.Frame), speed=$([math]::Sqrt($nearest.Speed2)) m/s, wheel-centre distance=$([math]::Sqrt($nearest.Distance2)) m, contacts=$($contacts -join ',')" }
    } }
    @{ Kind='LogCount'; Name='post-simulation suspension evidence captured'; Pattern='^\[wsus-post\]'; Min=20 }
)
$case
