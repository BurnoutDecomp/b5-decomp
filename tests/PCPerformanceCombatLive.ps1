# Workload qualification, not a clean FPS measurement: crash witnesses are on.
# Run via tools/tests/run_case.ps1 -Case b5-decomp/tests/PCPerformanceCombatLive.ps1 -Slot 8.
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'pc_performance_combat'
$case.Bug = 'A combat performance run must contain repeated player takedowns and tumbling rival wrecks.'
$case.Frames = $false
$case.Run.MaxSeconds = 150
$case.DiagEnv = 'BRN_AI_PAD_PLAYER=combat,BRN_AI_MADNESS=1,BRN_FRAME_PROFILE=1,BRN_RIVAL_DAMAGE_DIAG=1,BRN_TD_DIAG=1,BRN_CRASHCAM_DIAG=1'
$case.Checks += @(
    @{ Kind = 'Script'; Name = 'at least five player takedowns across three rivals'; Script = {
        param($ctx)
        $victims = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[rival-damage\] player-takedown victim=(\d+)') { [int]$Matches[1] }
        })
        $distinct = @($victims | Select-Object -Unique)
        @{ Pass = ($victims.Count -ge 5 -and $distinct.Count -ge 3)
           Detail = "$($victims.Count) player-credited takedowns across $($distinct.Count) rivals; generic crashes do not count" }
    } }
    @{ Kind = 'Script'; Name = 'multiple targeted rivals tumble or launch'; Script = {
        param($ctx)
        $credited = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[rival-damage\] player-takedown victim=(\d+)') { [int]$Matches[1] }
        } | Select-Object -Unique)
        $flying = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[td-flight\] slot=(\d+) f=\d+ crashing=1 .* vel=[^,]+,([^,]+),[^ ]+ upy=([^ ]+)') {
                $slot = [int]$Matches[1]
                $vertical = [double]::Parse($Matches[2], [cultureinfo]::InvariantCulture)
                $up = [double]::Parse($Matches[3], [cultureinfo]::InvariantCulture)
                if ($credited -contains $slot -and ($vertical -gt 3.0 -or $up -lt 0.0)) { $slot }
            }
        } | Select-Object -Unique)
        @{ Pass = $flying.Count -ge 2; Detail = "$($flying.Count) credited rival slots with launch/roll evidence" }
    } }
)
$case
