# Ordinary Road Rage input, including the south-to-west Waterfront bend.
$case = & "$PSScriptRoot/RivalOrganic.ps1"
$case.Name = 'rival_road'
$case.Bug = 'Rivals must drive around the Waterfront bend without teleporting through it or piling into its wall.'
$case.Run.MaxSeconds = 90
$case.Run.Boost = ''
$case.Run.FrameEvery = 30
$case.Run.MinFreeGB = 2
$case.Frames = $true
$case.DiagEnv = 'BRN_AI_ROAD_DIAG=1'
$case.Checks = @(
    @{ Kind='NewAsserts'; Name='no new assertions' }
    @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
    @{ Kind='Mark'; Name='reached driving'; Phase='DRIVING' }
    @{ Kind='LogMatch'; Name='road steering sampled'; Pattern='\[ai-road\]'; Expect=$true }
    @{ Kind='Script'; Name='rivals drive through the bend on the road'; Script={ param($ctx)
        $previous = @{}
        $approaching = @{}
        $completed = @{}
        foreach ($line in $ctx.LogLines) {
            if ($line -notmatch '\[ai-road\] car=(\d+) pos=([^ ]+) dir=([^ ]+) speed=([^ ]+)') { continue }
            $car = [int]$Matches[1]
            $position = @($Matches[2].Split(',') | ForEach-Object { [double]::Parse($_, [Globalization.CultureInfo]::InvariantCulture) })
            $direction = @($Matches[3].Split(',') | ForEach-Object { [double]::Parse($_, [Globalization.CultureInfo]::InvariantCulture) })
            $speed = [double]::Parse($Matches[4], [Globalization.CultureInfo]::InvariantCulture)
            if ($previous.ContainsKey($car)) {
                $dx = $position[0] - $previous[$car][0]
                $dz = $position[2] - $previous[$car][2]
                # Samples are 60 steering updates apart. A placement jump cannot satisfy the bend.
                if ($dx*$dx + $dz*$dz -gt 120*120) { $approaching.Remove($car) }
            }
            $previous[$car] = $position
            if ($position[0] -gt 3000 -and $position[2] -gt -2050 -and $direction[1] -lt -0.5) {
                $approaching[$car] = $true
            }
            # Exit gate spans the westbound carriageway, beyond the turn and before the next bend.
            if ($approaching.ContainsKey($car) -and $position[0] -lt 2885 -and $position[0] -gt 2780 -and
                $position[2] -gt -2190 -and $position[2] -lt -2130 -and $direction[0] -lt -0.7 -and $speed -gt 15) {
                $completed[$car] = $true
            }
        }
        # Allow one rival to be delayed by ordinary Road Rage contact.
        @{ Pass=$completed.Count -ge 4; Detail="$($completed.Count) rivals drove south-to-west through the road exit gate; require at least 4" }
    } }
)
$case
