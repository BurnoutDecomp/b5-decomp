# FX-AGG live case, slam variant (crash parity 2026-09-22): AIAggressionLive.ps1 plus BRN_AI_MADNESS=1,
# the existing PC harness override in AICar::SetRoadRageMadness (madness 1.0 -> aggression 0.6, a 60%
# DecideToAttack roll instead of a fresh profile's ~6-10%). The slam chain OVERTAKE_TO_SLAM /
# DROP_BACK_TO_SLAM / ATTACK_SLAM then runs within one organic pursuit, and the lineup side check covers
# ATTACK_SLAM's -8: GetPositionNextToTarget @0x827714E8 must put it on the far side of the player.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/AIAggressionSlamLive.ps1 --run-name ai_aggression_slam_live
# ATTACK_SLAM is reached only through DROP_BACK_TO_SLAM -> CanSlam, i.e. a rival that decided to attack
# while more than 4 m AHEAD of the player and then dropped back into the (-3, 2) m lead band. First run
# (scratch/bugtest/runs/ai_aggression_slam_live/20260922_215824, 2026-09-22): DecideToAttack 4/11, but
# the pad pursuit left the player slow or stopped, every attack started level or behind and went to
# OVERTAKE_TO_SLAM (5 lineup samples, all on the console side) -- no ATTACK_SLAM, so its check did not
# fire. That is "no evidence", not a pass; it needs a drive that keeps the player at speed ahead of
# nobody (e.g. a straight -SteerScript run) to bring rivals in from in front.
$case = & (Join-Path $PSScriptRoot 'AIAggressionLive.ps1')
$case.Name = 'ai_aggression_slam_live'
$case.Bug = 'With the slam chain armed, rivals must line up for ATTACK_SLAM on the far side of the player, with no new assertions.'
$case.DiagEnv += ',BRN_AI_MADNESS=1'
$case.Checks += @(
    @{ Kind = 'Script'; Name = 'ATTACK_SLAM lineup sampled on the far side of the player'; Script = {
        param($ctx)
        $far = 0; $near = 0
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[mm-ai\] lineup car (\d+) aggState 3 pointLat ([^ ]+) carLat ([^ ]+)') {
                $point = [double]::Parse($Matches[2], [cultureinfo]::InvariantCulture)
                $car = [double]::Parse($Matches[3], [cultureinfo]::InvariantCulture)
                if ([math]::Abs([math]::Abs($point) - 8.0) -gt 0.2 -or [math]::Abs($car) -lt 0.5) { continue }
                if (($point * $car) -lt 0) { $far++ } else { $near++ }
            }
        }
        @{ Pass = ($far -gt 0 -and $near -eq 0); Detail = "ATTACK_SLAM samples: far side $far, rival's own side $near" }
    } }
)
$case
