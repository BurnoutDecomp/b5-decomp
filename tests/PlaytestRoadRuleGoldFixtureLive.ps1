# Explicitly synthetic both-rule profile. This tests loading/rendering/retention
# of gold, not earning a Showtime score. The actual time-win case is separate.
$case = & (Join-Path $PSScriptRoot 'PlaytestRoadRuleWinRebootLive.ps1')
$case.Name = 'playtest_road_rule_gold_fixture'
$case.Bug = 'A saved both-rule fixture must produce a gold road plate and retain both scores.'
$case.ScoreProvenance = 'synthetic fixture: earned time plus injected crash score'
$case.ProfileFixture = (Resolve-Path (Join-Path $PSScriptRoot '../../scratch/PLAYTEST_1005/triggers/synthetic_nakamura_gold.sav')).Path
$case.Checks = @($case.Checks | Where-Object { $_.Name -ne 'saved win drives silver plate and target' })
$case.Checks += @(
    @{Kind='LogMatch';Name='both saved rules drive gold plate and target';Pattern='\[road-plate\] road=396114 .*colour=3 bestTime=2966 leader=1'}
)
$case
