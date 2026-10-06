# Ordinary pad inputs lay a short turn/brake strip, then hold the car at rest.
# The diagnostic witnesses positions/timestamps without changing depth/blend/colour.
@{
    Name = 'playtest_render_skid_hold_1005'
    Area = 'vfx'
    Bug = 'Follow one world skid strip under a settled chase camera through its original ten-second age/fade.'
    Frames = $true
    ProfileFixture = 'rival_hunt_profile.sav'
    Run = @{
        Drive = $true
        DriveDelay = 12
        MotionProbe = $true
        SkipIntro = $true
        AcceptGap = 1.0
        Teleport = '3040.7,-5.8,-1937.9,180'
        SteerScript = '0:none,4:right50,5.5:none'
        ThrottleScript = '0:accel,4:accel+handbrake,5.5:brake,8:handbrake'
        MaxSeconds = 85
        FrameEvery = 6
    }
    DiagEnv = 'BRN_SKID_PROBE=1,BRN_TRAIL_HEIGHT_DIAG=1,BRN_SKID_LOUD=0,BRN_SKID_LIFT=0,BRN_SHADOW_PROBE=1,BRN_ENVMAP_STATS=1,BRN_FRAME_DUMP_ARM=skid,BRN_FRAME_DUMP_MAX=1000'
    Checks = @(
        @{ Kind='NewAsserts'; Name='no new assertions' }
        @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
        @{ Kind='Mark'; Name='reached driving'; Phase='DRIVING' }
        @{ Kind='LogMatch'; Name='actual skid vertices recorded'; Pattern='\[trailquad\].*type=\d+ emitter=.*laid=[\d.]+ now=[\d.]+' }
        @{ Kind='LogMatch'; Name='original aged strip recorded'; Pattern='\[trailquad\].*age=0\.[89]\d\d' }
        @{ Kind='LogCount'; Name='no loud-state override'; Pattern='\[trailpass\] BRN_SKID_LOUD=.*ARMED'; Max=0 }
        @{ Kind='LogCount'; Name='no height override'; Pattern='\[trailpass\] BRN_SKID_LIFT=.*ARMED'; Max=0 }
    )
}
