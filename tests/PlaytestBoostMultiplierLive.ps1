# Run through the usual run_case.ps1 on an isolated slot.
# The existing debug car-change path selects an aggression vehicle; boost gains
# and multiplier changes must come from ordinary collision/scoring and pad input.
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'playtest_boost_multiplier'
$case.Area = 'ui/boost'
$case.Bug = 'The aggression boost multiplier must use both original texture masks, without a solid rectangular flame.'
$case.ProfileFixture = 'rival_hunt_profile.sav'
$case.Frames = $true
$case.Run.MaxSeconds = 140
$case.Run.FrameEvery = 80
$case.DiagEnv += ',BRN_DEBUG_PLAYER_CAR=PUSCLT02,BRN_BOOST_TICKER_DIAG=1,BRN_AI_PAD_PLAYER=pursuit,BRN_FRAME_DUMP_MAX=320'
$case.Checks = @(
    @{ Kind='Mark'; Name='reached driving'; Phase='DRIVING' }
    @{ Kind='LogCount'; Name='no assertions'; Pattern='\[ASSERT(?:\s|\])'; Max=0 }
    @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
    @{ Kind='LogMatch'; Name='aggression vehicle selected through original debug event'; Pattern='\[car\] \*\*\*\*\* HARNESS-ONLY PLAYER CAR SWAP' }
    @{ Kind='LogMatch'; Name='original AI seat drives through pad input'; Pattern='\[ai-pad\] \*\*\*\*\* HARNESS-ONLY \(BRN_AI_PAD_PLAYER=pursuit\): armed' }
    @{ Kind='LogMatch'; Name='actual aggression multiplier rendered'; Pattern='\[boost-bar\] 206 .*allowed=1 type=1 .*status=3 mult=[12]' }
    @{ Kind='Script'; Name='multiplier capture contains actual frames'; Script={
        param($ctx)
        $count = @(Get-ChildItem -LiteralPath (Join-Path $ctx.RunDir 'frames') -Filter '*.bmp').Count
        @{Pass=($count -ge 10); Detail="$count actual frames; inspect flame silhouette and multiplier, not just diagnostic state."}
    } }
)
$case
