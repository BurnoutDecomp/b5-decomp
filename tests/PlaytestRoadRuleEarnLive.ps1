# The staged data places Nakamura Avenue (road37) between limits396111/396112,
# 230.31m apart, with the authored time par4699ms. The real AI pad follows the
# road; no completion or score is injected. A second boot must separately
# verify the saved result and visible silver plate.
@{
    Name = 'playtest_road_rule_earn'
    Area = 'freeroam/road_rules'
    Bug = 'An earned time road rule must reach the profile save and its silver road plate.'
    ProfileFixture = 'rival_hunt_profile.sav'
    SaveCompareScript = Join-Path $PSScriptRoot 'playtest_road_rule_save.py'
    Frames = $true
    Run = @{
        Drive = $true
        MotionProbe = $true
        SkipIntro = $true
        AcceptGap = 1.0
        # AI section210/span145 entry portal is (1453.154,3.287,-2193.033).
        # Start30m before it along the authored entry-box heading0.21765rad.
        CrashSweep = '1446.676,3.287,-2222.325'
        CrashSweepShots = '12.4704:0,12.4704:10'
        CrashSweepSettle = 900
        MenuTapAt = '1:EventDetails'
        MaxSeconds = 115
        FrameEvery = 60
    }
    DiagEnv = 'BRN_ROADRULES_DIAG=1,BRN_ROAD_PLATE_DIAG=1,BRN_AI_PAD_PLAYER=cruise,BRN_SWEEP_WAIT_ROAMING=1,BRN_FRAME_DUMP_MAX=330'
    Checks = @(
        @{ Kind='Mark'; Name='reached driving'; Phase='DRIVING' }
        @{ Kind='LogCount'; Name='no assertions'; Pattern='\[ASSERT(?:\s|\])'; Max=0 }
        @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
        @{ Kind='LogMatch'; Name='time road rules activated'; Pattern='\[roadrules\] action 282 -> gui 343 rule 1' }
        @{ Kind='LogMatch'; Name='measured approach seated on road'; Pattern='\[sweep\] seat ok shot 1' }
        @{ Kind='LogMatch'; Name='a time attempt completed with a real valid score'; Pattern='\[roadrules\] action 278 -> gui \d+ type 0 score .* valid 1' }
        @{ Kind='Script'; Name='earned time score persisted below the authored par'; Script={
            param($ctx)
            $root = Split-Path (Split-Path $ctx.Case.SaveCompareScript -Parent) -Parent
            $root = Split-Path $root -Parent
            $before = Join-Path $root 'tools/tests/fixtures/rival_hunt_profile.sav'
            $after = Join-Path $ctx.RunDir 'Profile.sav.after'
            $raw = & py -3 $ctx.Case.SaveCompareScript $before $after 2>&1
            try { $data = ("$raw" | ConvertFrom-Json) }
            catch { return @{Pass=$false; Detail="Save comparison could not read the actual files: $raw"} }
            "$raw" | Set-Content -LiteralPath (Join-Path $ctx.RunDir 'road-scores.json') -Encoding UTF8
            @{Pass=[bool]$data.pass; Detail="$($data.new_times_below_authored_par) newly saved time records below4699ms in slot37; $($data.lost.Count) prior scores lost. Visible plate and second boot remain separate checks."}
        } }
    )
}
