# Boot the actual save produced by the earned-rule case. Both files remain
# untouched: run_case copies this fixture into a protected private slot.
$case = & (Join-Path $PSScriptRoot 'PlaytestRoadRuleEarnLive.ps1')
$case.Name = 'playtest_road_rule_reboot'
$case.Bug = 'An earned road-rule record must survive a real second boot and still drive the road plate.'
$case.ProfileFixture = (Resolve-Path (Join-Path $PSScriptRoot '../../scratch/PLAYTEST_1005/road-rule-earn-batch9/Profile.sav.after')).Path
$case.Run.MaxSeconds = 75
$case.Checks = @($case.Checks | Where-Object { $_.Kind -ne 'Script' })
$case.Checks += @(
    @{ Kind='Script'; Name='saved road scores survive second boot'; Script={
        param($ctx)
        $raw = & py -3 $ctx.Case.SaveCompareScript $ctx.Case.ProfileFixture (Join-Path $ctx.RunDir 'Profile.sav.after') --retained 2>&1
        try { $data = ("$raw" | ConvertFrom-Json) }
        catch { return @{Pass=$false; Detail="Save comparison could not read the actual files: $raw"} }
        "$raw" | Set-Content -LiteralPath (Join-Path $ctx.RunDir 'road-scores.json') -Encoding UTF8
        $provenance = if ($ctx.Case.ScoreProvenance) { $ctx.Case.ScoreProvenance } else { 'genuinely earned' }
        @{Pass=[bool]$data.pass; Detail="$($data.existing_scores) saved score records ($provenance); $($data.lost.Count) missing or worse after second boot. Check captured plate separately."}
    } }
)
$case
