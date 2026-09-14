# Run with the parent workflow's tools/tests/run_case.ps1.
$case = & (Join-Path $PSScriptRoot 'RivalCamera.ps1')
$case.Name = 'player_camera_handoff'
$case.Bug = 'Player AI retains road-following and speed during a takedown camera.'
# The restored rolling-start AI moves the player earlier. Trigger before the
# straight scripted drive reaches its first collision, to isolate camera control.
$case.DiagEnv = $case.DiagEnv.Replace('BRN_FORCE_TAKEDOWN=12','BRN_FORCE_TAKEDOWN=4')
$case.DiagEnv += ',BRN_PLAYER_AI_DIAG=1,BRN_ONCOMING_DIAG=1'
$case.Run.MaxSeconds = 55
$case.Checks += @(
    @{ Kind='Script'; Name='player retains speed during takedown and regains control'; Script={
        param($ctx)
        $inside=$false; $finished=$false; $returned=$false; $samples=@()
        foreach($line in $ctx.LogLines) {
            if (!$finished -and $line -match '\[crashcam\] container current state -> 3\b') { $inside=$true }
            elseif ($inside -and $line -match '\[crashcam\] container current state -> 1\b') { $inside=$false; $finished=$true }
            if ($line -match '\[player-ai\] control=(\d) speed=([\d.]+).* brake=([\d.]+)') {
                if ($inside) { $samples += @{ Control=[int]$Matches[1]; Speed=[double]::Parse($Matches[2],[cultureinfo]::InvariantCulture); Brake=[double]::Parse($Matches[3],[cultureinfo]::InvariantCulture) } }
                if ($finished -and $Matches[1] -eq '1') { $returned=$true }
            }
        }
        if ($samples.Count -lt 3) { return @{Pass=$false;Detail='No complete takedown control sample window'} }
        $start=$samples[0].Speed; $end=$samples[-1].Speed
        $owned=@($samples | Where-Object Control -eq 0).Count
        @{Pass=($returned -and $start -ge 20 -and $end -ge ($start*0.85) -and $owned -ge ($samples.Count-1)); Detail="AI samples $owned/$($samples.Count), speed $start -> $end m/s, player returned=$returned"}
    } }
)
$case
