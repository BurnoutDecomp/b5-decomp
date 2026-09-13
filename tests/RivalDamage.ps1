$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'rival_damage'
$case.Bug = 'A player-credited rival takedown must show a moving, physically crashing, deformed victim.'
$case.Frames = $true
$case.Run.FrameEvery = 12
$case.Run.MinFreeGB = 2
$case.DiagEnv += ',BRN_RIVAL_DAMAGE_DIAG=1,BRN_CAMERA_RIG_DIAG=1'
$case.Checks += @(
    @{ Kind = 'Script'; Name = 'credited rival physically crashes with damage rendering enabled'; Script = {
        param($ctx)
        $victims = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[rival-damage\] player-takedown victim=(\d+)') { [int]$Matches[1] }
        } | Select-Object -Unique)
        $samples = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[rival-damage\] slot=(\d+) crashing=1 damaged=1 displacement=([^ ]+) offsets=([^ ]+) pos=([^ ]+) angular=([^ ]+)') {
                $slot = [int]$Matches[1]
                $displacement = [double]::Parse($Matches[2], [cultureinfo]::InvariantCulture)
                $offsets = [double]::Parse($Matches[3], [cultureinfo]::InvariantCulture)
                if ($victims -contains $slot -and $displacement -gt 0.0001 -and $offsets -gt 0.001) {
                    "$slot $($Matches[4]) $($Matches[5])"
                }
            }
        } | Select-Object -Unique)
        @{ Pass = $samples.Count -gt 1; Detail = "$($samples.Count) distinct moving/deformed samples for player-credited victims" }
    } }
)
$case
