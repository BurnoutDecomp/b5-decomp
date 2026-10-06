# The returning fixture has five discovered service sites, all outside its first
# harbour zoom window. Hold the original LT zoom-out while the service-only
# filter is active, so visible services are a controlled requirement.
$case = & (Join-Path $PSScriptRoot 'PlaytestFullMapLive.ps1') -Entrance Direct
$case.Name = 'playtest_full_map_drive_filter'
$case.Bug = 'Direct-entry drive-through filter must show discovered service markers inside the original zoomed-out viewport.'
$case.Run.MaxSeconds = 80
$case.Run.FrameEvery = 30
$case.Run.MenuScript = "timeout:25;sleep:4;tap:PauseMap;wait:\[screen\] ENTER 'CN_MAP_MAIN;sleep:4;mark:direct-map;tap:DPadRight;wait:\[cnav-diag\] SetFilterFromPanel: panel=1;sleep:2;mark:drive-filter-local;hold:GuiZoom:6;sleep:2;mark:drive-filter-zoomed;sleep:5;mark:drive-filter-restored;tap:Stop"
$case.DiagEnv = 'BRN_SCREEN_DIAG=1,BRN_SATNAV_DIAG=1,BRN_HUD_VIS=1,BRN_APT_PROMPT_DIAG=1,BRN_APT_TEXT_LAYOUT_DIAG=1,BRN_FRAME_DUMP_MAX=430'
$case.Checks = @(
    @{Kind='Mark';Name='reached free roam';Phase='DRIVING'}
    @{Kind='LogMatch';Name='direct Back map opens';Pattern="\[screen\] ENTER 'CN_MAP_MAIN.*from 'INGAME"}
    @{Kind='LogMatch';Name='service-only filter selected';Pattern='\[cnav-diag\] SetFilterFromPanel: panel=1 displayType=5 iconFilter=0 roadSigns=0 driveThrus=1'}
    @{Kind='LogMatch';Name='original LT zoom-out held';Pattern='\[cnav-diag\] map input event 6 action 56'}
    @{Kind='Script';Name='discovered service bank appears while zoom is held under the service filter';Script={
        param($ctx)
        $inFilter=$false; $holding=$false; $rows=0; $peak=0
        foreach($line in $ctx.LogLines) {
            if($line -match '\[cnav-diag\] SetFilterFromPanel: panel=(?<panel>\d+)') {$inFilter=($Matches.panel -eq '1')}
            if($line -match '\[cnav-diag\] map input event 6 action 56') {$holding=$true}
            if($line -match '\[cnav-diag\] map input event 7 action 56') {$holding=$false}
            if($inFilter -and $holding -and $line -match '^\[cnav-bank\] player=(?<player>\d+) rivals=\d+ drive=(?<drive>\d+)') {
                if([int]$Matches.player -ge 1 -and [int]$Matches.drive -ge 1) {$rows++;$peak=[math]::Max($peak,[int]$Matches.drive)}
            }
        }
        @{Pass=($rows -ge 2);Detail="service-filter held-zoom rows containing player/services=$rows; peak services=$peak; actual marker pixels require frame review"}
    }}
    @{Kind='Script';Name='local, zoomed and restored filter views captured';Script={
        param($ctx)
        @{Pass=($ctx.MarksText -match 'mark drive-filter-local' -and $ctx.MarksText -match 'mark drive-filter-zoomed' -and $ctx.MarksText -match 'mark drive-filter-restored');Detail='Original direct entry, filter and held LT zoom stimulus completed.'}
    }}
    @{Kind='LogCount';Name='no assertions';Pattern='\[ASSERT(?:\s|\])';Max=0}
    @{Kind='LogCount';Name='no exceptions';Pattern='\[EXCEPTION\]';Max=0}
)
$case
