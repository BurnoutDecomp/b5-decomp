# Bounded ordinary-pad capture for exhaust, skid strips, bloom and motion blur.
# The returning profile and slot fixture protect the owner's save/config.
@{
    Name = 'playtest_render_driving_1005'
    Area = 'vfx'
    Bug = 'Inspect exhaust attachment, stable world skid strips and original post-fx while the actual player drives and boosts.'
    Frames = $true
    ProfileFixture = 'rival_hunt_profile.sav'
    Run = @{
        Drive = $true
        DriveDelay = 12
        MotionProbe = $true
        SkipIntro = $true
        AcceptGap = 1.0
        Teleport = '3040.7,-5.8,-1937.9,180'
        Boost = '2:6:2'
        SteerScript = '0:none,5:right25,7:none,10:left25,12:none,16:right50,18:none'
        ThrottleScript = '0:accel,6:accel+handbrake,7:accel,12:brake,13:accel'
        MaxSeconds = 85
        FrameEvery = 6
    }
    DiagEnv = 'BRN_BOOSTLOC_DIAG=1,BRN_DEFORMLOC_DIAG=1,BRN_SKID_PROBE=1,BRN_SHADOW_PROBE=1,BRN_ENVMAP_STATS=1,BRN_FRAME_DUMP_ARM=skid,BRN_FRAME_DUMP_MAX=600'
    Checks = @(
        @{ Kind='NewAsserts'; Name='no new assertions' }
        @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
        @{ Kind='Mark'; Name='reached driving'; Phase='DRIVING' }
        @{ Kind='LogMatch'; Name='real deformation exhaust locator output exists'; Pattern='\[boostloc\] tags=[1-4] activeMask=0[1-9A-F]' }
        @{ Kind='LogMatch'; Name='a real authored boost effect starts'; Pattern='\[boostfx\].*StartEffects\(".*Boost(Yellow|Red|Green)' }
        @{ Kind='LogCount'; Name='no locator stand-in'; Pattern='\[boostloc\] STAND-IN'; Max=0 }
    )
}
