# Same original PUSCLT02 boost/rival route that showed the diagonal sky band.
# Optional witnesses select logs only; all renderer/effect values are preserved.
$case = & (Join-Path $PSScriptRoot 'PlaytestBoostMultiplierLive.ps1')
$case.Name = 'playtest_render_boost_lines_1005'
$case.Area = 'vfx'
$case.Bug = 'Identify the actual boosted sky ribbons from projected particle corners and material bindings.'
$case.Run.MaxSeconds = 140
$case.Run.FrameEvery = 24
$case.DiagEnv += ',BRN_LIONQUAD_EXCLUDE=SQUARELIGHT,BRN_LIONQUAD_CLIP=1,BRN_BOOSTLOC_DIAG=1,BRN_DEFORMLOC_DIAG=1,BRN_FRAME_DUMP_ARM=skid,BRN_FRAME_DUMP_MAX=1000'
# The observer uses this run's unique FrameDir/inputs, preserving provenance.
$case.DiagEnv += ',BRN_POSTFX_SOURCE_DUMP=1'
$case.Checks = @(
    @{ Kind='NewAsserts'; Name='no new assertions' }
    @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
    @{ Kind='Mark'; Name='reached driving'; Phase='DRIVING' }
    @{ Kind='LogMatch'; Name='actual aggression truck selected'; Pattern='\[car\].*HARNESS-ONLY PLAYER CAR SWAP.*US Classic Truck' }
    @{ Kind='LogMatch'; Name='actual event HUD reaches race state'; Pattern='\[hud-reveal\] RACE_MAIN UpdateWFInit inEvent=1' }
    @{ Kind='LogMatch'; Name='actual player four-tag aggression boost starts'; Pattern='\[boostfx\].*StartEffects\(".*BoostRed.*muNumBoostTags=4' }
    @{ Kind='LogMatch'; Name='projected particle witness exists'; Pattern='\[lionclip\]' }
    @{ Kind='LogCount'; Name='no locator stand-in'; Pattern='\[boostloc\] STAND-IN'; Max=0 }
    @{ Kind='LogMatch'; Name='actual precomposite source paired to frames'; Pattern='\[postfx-source\].*unit=0 .*written=1' }
)
$case
