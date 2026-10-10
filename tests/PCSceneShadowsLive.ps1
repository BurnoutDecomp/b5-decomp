# Native capture/caster witness using the existing deterministic wall-crash case.
# Run with tools/tests/run_case.ps1 -Case b5-decomp/tests/PCSceneShadowsLive.ps1.
# Requires the enabled Relative [Debug] preset in config.ini beside the executable.
# Counts are native submissions, not an assertion of visual parity.
@{
    Name='pc_scene_shadows'
    Area='graphics'
    Bug='Published crash debris must reach optional cascade casters and cube particle draws without asserts or exhausting the shared immediate arena.'
    Frames=$false
    Run=@{
        Drive=$true
        MaxSeconds=75
        CrashSweep='3249.796,-3.7,-1925.404'
        CrashSweepShots='225:70'
        CrashSweepArm=4
    }
    DiagEnv='BRN_SMALL_SHADOW_TRACE=1,BRN_REFLECTION_SCENE_TRACE=1,BRN_DEBRIS_BURST_DIAG=1'
    Checks=@(
        @{Kind='Mark';Name='reached driving';Phase='DRIVING'}
        @{Kind='LogCount';Name='the deterministic wall shot fired';Pattern='\[sweep\] shot 0/';Min=1}
        @{Kind='LogCount';Name='solid debris is submitted to the native shadow cascades';Pattern='\[small-shadow\] cascade=[0-2] solidDebris=[1-9][0-9]* ';Min=1}
        @{Kind='LogCount';Name='published particles produce reflected vertices';Pattern='\[reflection-scene\] face=[0-5] .*particleBytes=[1-9][0-9]*';Min=1}
        @{Kind='LogCount';Name='per-face buffers do not exhaust the original immediate arena';Pattern='arena exhausted|arena exhaustion';Max=0}
        @{Kind='LogCount';Name='zero asserts';Pattern='\[ASSERT [0-9]+\]';Max=0}
        @{Kind='LogCount';Name='zero exceptions';Pattern='\[EXCEPTION\]';Max=0}
    )
}
