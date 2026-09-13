# Full-game regression case driven by tests/run_rival_organic.py.
# The driver uses normal pad input to pursue a rival. No injected takedown or AI override.
@{
    Name = 'rival_organic'
    Area = 'takedown'
    Bug = 'Real rival collisions must reach impact detection, player credit, HUD, AI and camera.'
    Frames = $false
    Run = @{
        Drive = $true
        Boost = '6:3:2'
        MotionProbe = $true
        SkipIntro = $true
        AcceptGap = 1.0
        Teleport = '3040.7,-5.8,-1937.9,180'
        StartEvent = $true
        EventFsm = $true
        SkipTrainingTip = $true
        MaxSeconds = 100
    }
    DiagEnv = 'BRN_CRASHCAM_DIAG=1,BRN_PROP_DIAG=1,BRN_TRAFFIC_DIAG=1,BRN_VFXFEED_PROBE=1'
    Checks = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
        @{ Kind = 'LogCount'; Name = 'no injected takedown'; Pattern = '\[td\] HARNESS'; Max = 0 }
        @{ Kind = 'LogMatch'; Name = 'real race-car contact'; Pattern = '\[td-contact\] entry race car'; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'impact classified'; Pattern = '\[td-contact\] verdict'; Expect = $true }
        @{ Kind = 'LogValue'; Name = 'takedown counted'; Pattern = '\[td\] (?<n>\d+) takedown event\(s\) this frame'; Group = 'n'; Agg = 'max'; Min = 1 }
        @{ Kind = 'LogMatch'; Name = 'player credit reached AI'; Pattern = '\[ai-evt\] action 14 ON_PLAYER_TAKEDOWN'; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'player takedown HUD message'; Pattern = 'HUD MSGS : STARTING MESSAGE NAMED "TDGD'; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'takedown camera'; Pattern = '\[crashcam\] container current state -> 3\b'; Expect = $true }
    )
}
