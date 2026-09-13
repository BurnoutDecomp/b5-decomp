# Run through the workflow's tools/tests/run_case.ps1.
$workflow = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$case = & (Join-Path $workflow 'tools/tests/cases/takedown_forced.ps1')
$case.Name = 'rival_camera_restored'
$case.Bug = 'Takedown rigs must move, and live speed must drive authored camera shake.'
$case.Frames = $true
$case.Run.MaxSeconds = 75
$case.Run.FrameEvery = 12
$case.Run.MinFreeGB = 2
$case.Run.Boost = '6:3:2'
$case.DiagEnv += ',BRN_TRAFFIC_DIAG=1,BRN_CAMERA_RIG_DIAG=1,BRN_DIRECTOR_TRACE=1'
$case.Checks += @(
    @{ Kind = 'Script'; Name = 'takedown rig tracks the moving rival'; Script = {
        param($ctx)
        $poses = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[camera-rig\] gyro car=\d+ eye=([^ ]+) target=([^ ]+) fov=([\d.]+)') {
                if ([double]::Parse($Matches[3], [cultureinfo]::InvariantCulture) -gt 0) { $Matches[1] }
            }
        } | Select-Object -Unique)
        @{ Pass = $poses.Count -gt 1; Detail = "$($poses.Count) distinct tracked camera positions" }
    } }
    @{ Kind = 'Script'; Name = 'authored shake produces nonzero camera rotation'; Script = {
        param($ctx)
        $moving = @($ctx.LogLines | Where-Object {
            if ($_ -match '\[camera-rig\] boost-shake type=\d+ keys=(\d+) frame=[^ ]+ angles=([^,]+),([^,]+),([^ ]+)') {
                $keys = [int]$Matches[1]
                $x = [double]::Parse($Matches[2], [cultureinfo]::InvariantCulture)
                $y = [double]::Parse($Matches[3], [cultureinfo]::InvariantCulture)
                $z = [double]::Parse($Matches[4], [cultureinfo]::InvariantCulture)
                $keys -gt 1 -and ([math]::Abs($x) + [math]::Abs($y) + [math]::Abs($z)) -gt 0.000001
            }
        })
        @{ Pass = $moving.Count -gt 1; Detail = "$($moving.Count) frames with sampled camera rotation" }
    } }
)
$case
