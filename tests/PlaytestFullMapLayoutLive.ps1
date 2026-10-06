$lCase=& (Join-Path $PSScriptRoot 'PlaytestFullMapTabsLive.ps1')
$lCase.Name='playtest_full_map_legend_layout'
$lCase.Bug='Correlate native Apt field boxes, host font metrics and actual draw matrices for the overlapping legend captions.'
$lCase.DiagEnv+=',BRN_APT_TEXT_LAYOUT_DIAG=1'
$lCase.Checks+=@(
    @{Kind='LogMatch';Name='caption allocation metrics recorded';Pattern='\[apt-text-layout\] alloc '}
    @{Kind='LogMatch';Name='native field/matrix fold recorded';Pattern='\[apt-text-layout\] fold '}
    @{Kind='LogMatch';Name='actual caption draw transform recorded';Pattern='\[apt-text-layout\] draw '}
    @{Kind='LogCount';Name='layout observations remain bounded';Pattern='\[apt-text-layout\]';Max=128}
)
return $lCase
