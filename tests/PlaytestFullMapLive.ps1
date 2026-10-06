param([ValidateSet('Direct','Tabs')][string]$Entrance = 'Direct')
# Exercise the actual CrashNav map and its D-pad filter controls on an isolated
# returning profile. Rendering commands alone do not establish visible blips.
$lsEnter = if ($Entrance -eq 'Direct') {
    'tap:PauseMap'
} else {
    "tap:Start;wait:\[screen\] ENTER 'CN_D_DETAIL;sleep:3;hold:ShoulderL:0.3"
}
@{
    Name = "playtest_full_map_$Entrance"
    Area = 'ui/satnav'
    Bug = 'Full-screen CrashNav blips, filter selection, inspection and navigation must remain usable.'
    ProfileFixture = 'rival_hunt_profile.sav'
    Frames = $true
    Run = @{
        MaxSeconds = 100
        SkipIntro = $true
        AcceptGap = 1.0
        FrameEvery = 90
        MenuScript = "timeout:25;sleep:4;$lsEnter;wait:\[screen\] ENTER 'CN_MAP_MAIN;sleep:5;mark:map-initial;tap:DPadDown;tap:DPadRight;sleep:3;mark:event-filter;tap:DPadLeft;tap:DPadUp;tap:DPadRight;sleep:4;mark:drivethru-filter;tap:DPadRight;sleep:4;mark:road-filter;tap:DPadRight;sleep:4;mark:events-return;hold:SteerRight:0.25;sleep:2;hold:StickUp:0.25;sleep:2;mark:icon-navigation;hold:GuiInspect:1.5;sleep:2;mark:inspection-release;hold:GuiZoom:1.5;sleep:2;mark:zoom-release;tap:Accept;sleep:2;tap:Stop;wait:\[screen\] ENTER 'INGAME;sleep:2;tap:PauseMap;wait:\[screen\] ENTER 'CN_MAP_MAIN;sleep:3;mark:map-reentered;tap:Stop"
    }
    DiagEnv = 'BRN_SCREEN_DIAG=1,BRN_SATNAV_DIAG=1,BRN_HUD_VIS=1,BRN_APT_COMPUPD=ButtonsAnimation,BRN_APT_PROMPT_DIAG=1,BRN_FRAME_DUMP_MAX=180'
    Checks = @(
        @{ Kind='Mark'; Name='reached free roam'; Phase='DRIVING' }
        @{ Kind='LogCount'; Name='map opens twice through original input'; Pattern="\[screen\] ENTER 'CN_MAP_MAIN"; Min=2 }
        @{ Kind='LogMatch'; Name='map components finish setup'; Pattern='\[cnav-diag\] CrashNavMap items loaded' }
        @{ Kind='LogMatch'; Name='event filter state is published'; Pattern='\[cnav-diag\] SetFilterFromPanel: panel=0' }
        @{ Kind='LogMatch'; Name='drive-through filter state is published'; Pattern='\[cnav-diag\] SetFilterFromPanel: panel=1' }
        @{ Kind='LogMatch'; Name='road-sign filter state is published'; Pattern='\[cnav-diag\] SetFilterFromPanel: panel=2' }
        @{ Kind='LogMatch'; Name='inspection uses the original right-trigger action'; Pattern='\[cnav-diag\] map input event 6 action 57' }
        @{ Kind='LogMatch'; Name='inspection releases through the original right-trigger action'; Pattern='\[cnav-diag\] map input event 7 action 57' }
        @{ Kind='LogMatch'; Name='zoom uses the original left-trigger action'; Pattern='\[cnav-diag\] map input event 6 action 56' }
        @{ Kind='LogCount'; Name='no asserts'; Pattern='\[ASSERT(?:\s|\])'; Max=0 }
        @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
        @{ Kind='Script'; Name='filter and reentry input sequence completed'; Script={
            param($ctx)
            @{Pass=($ctx.MarksText -match 'mark map-initial' -and $ctx.MarksText -match 'mark icon-navigation' -and $ctx.MarksText -match 'mark map-reentered'); Detail='Real D-pad filters, two-axis icon navigation, trigger inspection/zoom, selection, exit and reentry; visible blips require captured-frame review.'}
        } }
    )
}
