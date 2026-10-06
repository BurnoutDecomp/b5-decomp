# Enter the brightness screen through the normal pause/settings flow.
@{
    Name = 'playtest_brightness'
    Area = 'ui'
    Bug = 'Opening screen brightness must acquire and display the calibration card without asserting or crashing.'
    Frames = $true
    ProfileFixture = 'rival_hunt_profile.sav'
    Run = @{
        MaxSeconds = 75
        SkipIntro = $true
        AcceptGap = 1.0
        FrameEvery = 90
        MenuScript = "timeout:25;sleep:5;tap:Start;wait:\[screen\] ENTER 'CN_D_DETAIL;sleep:2;hold:ShoulderR:0.15;wait:\[screen\] ENTER 'CN_SETTINGS;sleep:2;tap:Nextx3;tap:Accept;wait:\[screen\] ENTER 'CN_COLOUR;wait:\[calib-screen\] calibration texture acquired;sleep:3;tap:OptionNext;tap:OptionPrev;sleep:2;tap:Stop;wait:\[calib-screen\] hidden;mark:brightness-complete"
    }
    DiagEnv = 'BRN_SCREEN_DIAG=1,BRN_FRAME_DUMP_MAX=120,BRN_BLACKBARS_DIAG=1,BRN_CAM_INPUT_DIAG=1'
    Checks = @(
        @{ Kind='LogMatch'; Name='entered calibration screen'; Pattern="\[screen\] ENTER 'CN_COLOUR" }
        @{ Kind='LogMatch'; Name='acquired calibration card'; Pattern='\[calib-screen\] calibration texture acquired' }
        @{ Kind='LogMatch'; Name='restored post effects after leaving'; Pattern='\[calib-screen\] hidden -- post-fx restored' }
        @{ Kind='LogCount'; Name='calibration card stays enabled between GUI ticks'; Pattern='\[calib\] composite override source ON'; Min=1; Max=1 }
        @{ Kind='LogCount'; Name='calibration card is removed once on exit'; Pattern='\[calib\] composite override source OFF'; Min=1; Max=1 }
        @{ Kind='LogCount'; Name='no asserts'; Pattern='\[ASSERT(?:\s|\])'; Max=0 }
        @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
    )
}
