# The car remains on the real simulation and camera path. Captures are bounded
# and start after returning-profile boot and streamed placement have settled.
@{
    Name = 'playtest_postfx_steady'
    Area = 'vfx'
    Bug = 'Inspect bloom and reflection stability with aligned scene/bloom/final captures; measured cadence and visible stability are separate from a log pass.'
    Frames = $true
    ProfileFixture = 'rival_hunt_profile.sav'
    Run = @{
        Drive = $true
        DriveDelay = 12
        MotionProbe = $true
        SkipIntro = $true
        AcceptGap = 1.0
        Teleport = '3040.7,-5.8,-1937.9,180'
        SteerScript = '0:none'
        ThrottleScript = '0:handbrake'
        MaxSeconds = 65
        FrameEvery = 1
    }
    # This long temporal diagnostic explicitly retains its historical 480 final
    # frames and 128 input pairs instead of inheriting the default cap of 30 pairs.
    DiagEnv = 'BRN_CAMERA_TRACE=1,BRN_SHADOW_PROBE=1,BRN_ENVMAP_STATS=1,BRN_FRAME_DUMP_ARM=0,BRN_FRAME_DUMP_START=6500,BRN_FRAME_DUMP_MAX=480,BRN_POSTFX_SOURCE_DUMP=1,BRN_POSTFX_SOURCE_START=6500,BRN_POSTFX_SOURCE_EVERY=5,BRN_POSTFX_SOURCE_MAX=128'
    Checks = @(
        @{ Kind='NewAsserts'; Name='no new assertions' }
        @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
        @{ Kind='Mark'; Name='reached driving'; Phase='DRIVING' }
        @{ Kind='LogCount'; Name='real scene readbacks'; Pattern='^\[postfx-source\] present=\d+ unit=0 .*written=1 '; Min=32 }
        @{ Kind='LogCount'; Name='real bloom readbacks'; Pattern='^\[postfx-source\] present=\d+ unit=1 .*written=1 '; Min=32 }
        @{ Kind='Script'; Name='bounded consecutive final captures begin at requested present'; Script={
            param($ctx)
            $frames = @(Get-ChildItem -LiteralPath $ctx.FrameDir -Filter 'bb_*.bmp' |
                ForEach-Object { if ($_.BaseName -match '^bb_(\d+)$') { [int]$Matches[1] } } |
                Sort-Object)
            $sequential = $true
            for ($i=1; $i -lt $frames.Count; ++$i) {
                if ($frames[$i] -ne $frames[$i-1]+1) { $sequential=$false; break }
            }
            @{ Pass=($frames.Count -ge 200 -and $frames.Count -le 480 -and $frames[0] -eq 6500 -and $sequential);
               Detail="final frames=$($frames.Count), first=$($frames[0]), last=$($frames[-1]), consecutive=$sequential; visual/cadence assessment follows" }
        } }
        @{ Kind='Script'; Name='car settles without acceleration'; Script={
            param($ctx)
            $rows = @($ctx.LogLines | Where-Object { $_ -match '^\[motion\]' } | Select-Object -Last 20)
            $speeds = @($rows | ForEach-Object { if ($_ -match '\|v\| ([\d.]+)') { [double]::Parse($Matches[1], [System.Globalization.CultureInfo]::InvariantCulture) } })
            $fast = @($speeds | Where-Object { $_ -gt 0.5 }).Count
            @{ Pass=($speeds.Count -ge 10 -and $fast -eq 0);
               Detail="last motion samples=$($speeds.Count), samples above0.5m/s=$fast; capture-time camera stability is reviewed separately" }
        } }
    )
}
