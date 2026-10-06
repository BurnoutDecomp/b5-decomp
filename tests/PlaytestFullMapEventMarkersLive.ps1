param([ValidateSet('MarkedMan','BurningRoute')][string]$Mode = 'MarkedMan')
# Reuse the original tested junction/event-start path. Never use WinTeleport or
# debug finish: pause the actual in-progress event and inspect its full map.
$file = if ($Mode -eq 'MarkedMan') {'marked_man_lifecycle.ps1'} else {'burning_route_lifecycle.ps1'}
$case = & (Join-Path $PSScriptRoot "../../tools/tests/cases/$file")
$case.Name = "playtest_full_map_$Mode"
$case.Area = 'ui/satnav'
$case.Bug = "Issue33: actual $Mode event must retain player, native rivals where present, and start/finish map markers."
$case.ExpectedGameMode = if ($Mode -eq 'MarkedMan') {8} else {5}
$case.ProfileFixture = 'rival_hunt_profile.sav'
$case.Frames = $true
$case.Run.Remove('WinTeleport')
$case.Run.MaxSeconds = 90
$case.Run.FrameEvery = 90
$case.Run.MenuScript = "timeout:50;wait:\[stunt\] mode state -> E_GMS_IN_PROGRESS;sleep:5;tap:PauseMap;wait:\[screen\] ENTER 'CN_MAP_MAIN;sleep:6;mark:event-map-initial;hold:GuiZoom:3;sleep:3;mark:event-map-wide;tap:DPadRight;sleep:3;tap:DPadLeft;sleep:3;mark:event-map-filter-return;tap:Stop"
$case.DiagEnv = 'BRN_SCREEN_DIAG=1,BRN_SATNAV_DIAG=1,BRN_HUD_VIS=1,BRN_INTRO_TIMER_DIAG=1,BRN_PROP_DIAG=1,BRN_MM_DIAG=1,BRN_FRAME_DUMP_MAX=240'
$case.Checks = @(
    @{Kind='Mark';Name='reached driving';Phase='DRIVING'}
    @{Kind='Script';Name='the actual intended event enters through its original intro';Script={
        param($ctx)
        $mode=[int]$ctx.Case.ExpectedGameMode; $count=0
        foreach($line in $ctx.LogLines) {
            if($line -match '\[mode-intro\] IntroState::OnEnter mode type (?<mode>\d+) ' -and [int]$Matches.mode -eq $mode) {$count++}
        }
        @{Pass=($count -ge 1);Detail="original mode type $mode intro count=$count"}
    }}
    @{Kind='LogMatch';Name='real event reaches in progress';Pattern='\[stunt\] mode state -> E_GMS_IN_PROGRESS'}
    @{Kind='LogMatch';Name='original pre-race159 enters map event colouring';Pattern='\[cnav-prerace\] cache159 messages=\d+ inEvent=1'}
    @{Kind='LogMatch';Name='direct map opens during the event';Pattern="\[screen\] ENTER 'CN_MAP_MAIN.*from 'INGAME"}
    @{Kind='Script';Name='actual event map has player, endpoints and original offline rival filtering';Script={
        param($ctx)
        $map=$false; $endpoints=0; $rivals=0
        foreach($line in $ctx.LogLines) {
            if($line -match "\[screen\] ENTER 'CN_MAP_MAIN") {$map=$true}
            if(!$map) {continue}
            if($line -match '^\[cnav-bank\] player=(?<player>\d+) rivals=(?<rivals>\d+) drive=\d+ start48=\d+ finish49=\d+ custom=(?<custom>\d+) inEvent=(?<event>[01])') {
                if([int]$Matches.player -ge 1 -and [int]$Matches.custom -ge 2 -and $Matches.event -eq '1') {$endpoints++}
                $rivals=[math]::Max($rivals,[int]$Matches.rivals)
            }
        }
        # ARTIST UpdateSatNavInfo825023D0 case3 filters offline rival records
        # in modes3/7/9/5/8. The mode5/8 comparisons at8250250C/14 branch
        # through82502578 to the skip at82502714. Hunters physically exist
        # in Marked Man, but retail deliberately does not show those rivals.
        $mode=[int]$ctx.Case.ExpectedGameMode
        $huntersAllowed=($mode -ne 8 -or @($ctx.LogLines | Where-Object {$_ -match '<AI> Allowed in Marked man :Yes,Yes,Yes,'}).Count -ge 1)
        @{Pass=($endpoints -ge 1 -and $rivals -eq 0 -and $huntersAllowed);Detail="event map rows with player/both endpoints=$endpoints, peak offline rival markers=$rivals (original modes5/8 expect0), actual hunters allowed=$huntersAllowed; pixels must establish endpoint placement/visibility"}
    }}
    @{Kind='Script';Name='initial, wide and restored event filters were captured';Script={
        param($ctx)
        @{Pass=($ctx.MarksText -match 'mark event-map-initial' -and $ctx.MarksText -match 'mark event-map-wide' -and $ctx.MarksText -match 'mark event-map-filter-return');Detail='Actual mode, direct map, held LT wide view and filter return completed.'}
    }}
    @{Kind='LogCount';Name='no forced finish or win teleport';Pattern='HARNESS DEBUG FINISH POSITION|\[win-teleport\] armed';Max=0}
    @{Kind='LogCount';Name='no assertions';Pattern='\[ASSERT(?:\s|\])';Max=0}
    @{Kind='LogCount';Name='no exceptions';Pattern='\[EXCEPTION\]';Max=0}
)
$case
