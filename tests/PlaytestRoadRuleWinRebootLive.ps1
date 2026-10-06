# Load the actual2916ms Nakamura win from the clean real-trigger case; no score edits.
$case = & (Join-Path $PSScriptRoot 'PlaytestRoadRuleRebootLive.ps1')
$case.Name = 'playtest_road_rule_win_reboot'
$case.Bug = 'A genuinely won rule must survive loading/autosave and retain its silver plate.'
$case.ProfileFixture = (Resolve-Path (Join-Path $PSScriptRoot '../../scratch/PLAYTEST_1005/road-rule-win-batch15/Profile.sav.after')).Path
$case.Checks += @(
    @{Kind='LogMatch';Name='saved win drives silver plate and target';Pattern='\[road-plate\] road=396114 .*colour=2 bestTime=2916 leader=1'}
)
$case
