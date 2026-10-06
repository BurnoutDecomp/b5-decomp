# Use the original settings-menu unlock sequence on an isolated fixture, then
# leave/re-enter settings so its selectable rows reflect the new credits flag.
@{
    Name = 'playtest_credits'
    Area = 'ui'
    Bug = 'Credits must mount their renderer, receive fonts, and display scrolling localized names.'
    ProfileFixture = 'rival_hunt_profile.sav'
    Frames = $true
    Run = @{
        MaxSeconds = 100
        SkipIntro = $true
        AcceptGap = 1.0
        FrameEvery = 90
        MenuScript = "timeout:25;sleep:5;tap:Start;wait:\[screen\] ENTER 'CN_D_DETAIL;sleep:2;hold:ShoulderR:0.15;wait:\[screen\] ENTER 'CN_SETTINGS;sleep:2;tap:Prev;tap:Next;tap:OptionPrevx2;tap:Prev;tap:Next;tap:OptionNextx2;sleep:1;tap:Stop;sleep:2;tap:Start;sleep:2;hold:ShoulderR:0.15;sleep:2;tap:Nextx3;tap:Accept;wait:\[screen\] ENTER 'CN_CREDITS;sleep:20;mark:credits-visible;tap:Accept;wait:\[hud-vis\] creditstext hide by evt=588;mark:credits-exit"
    }
    DiagEnv = 'BRN_SCREEN_DIAG=1,BRN_HUD_VIS=1,BRN_FRAME_DUMP_MAX=170,BRN_BLACKBARS_DIAG=1'
    Checks = @(
        @{ Kind='LogMatch'; Name='entered credits through settings'; Pattern="\[screen\] ENTER 'CN_CREDITS" }
        @{ Kind='LogMatch'; Name='credits renderer hides on exit'; Pattern='\[hud-vis\] creditstext hide by evt=588' }
        @{ Kind='LogCount'; Name='no asserts'; Pattern='\[ASSERT(?:\s|\])'; Max=0 }
        @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
        @{ Kind='Script'; Name='credits viewing and exit completed'; Script={
            param($ctx)
            @{Pass=($ctx.MarksText -match 'mark credits-visible' -and $ctx.MarksText -match 'mark credits-exit'); Detail='Full menu script, 20-second viewing interval and exit marks required; visible names checked in captured frames.'}
        } }
    )
}
