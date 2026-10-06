# Logs only unless the caller explicitly enables the bounded pixel window.
param([int]$Width=2560,[int]$Height=1440,[switch]$Capture,[int]$StartPresent=5000)
$case=@{
    Name='playtest_postfx_moving'
    Area='vfx'
    Bug='Observe actual moving postFX at the requested native resolution; log health and visible bloom stability are separate verdicts.'
    Frames=[bool]$Capture
    ProfileFixture='rival_hunt_profile.sav'
    Run=@{
        Drive=$true
        DriveDelay=12
        MotionProbe=$true
        SkipIntro=$true
        AcceptGap=1.0
        Teleport='3040.7,-5.8,-1937.9,180'
        SteerScript='0:none,5:right25,7:none,10:left25,12:none,16:right25,18:none'
        ThrottleScript='0:accel,20:brake+handbrake,23:handbrake'
        MaxSeconds=65
        FrameEvery=1
    }
    DiagEnv='BRN_CAMERA_TRACE=1'
    Checks=@(
        @{Kind='NewAsserts';Name='no new assertions'}
        @{Kind='LogCount';Name='no exceptions';Pattern='\[EXCEPTION\]';Max=0}
        @{Kind='Mark';Name='reached driving';Phase='DRIVING'}
        @{Kind='Script';Name='requested native scene target is live';Script={
            param($ctx)
            $width=$ctx.Case.NativeWidth; $height=$ctx.Case.NativeHeight
            $rows=@($ctx.LogLines | Where-Object {$_ -match '^\[postfx-rt\] default render-target state installed: \d+x\d+'})
            $matchesSize=$rows.Count -ge 1 -and $rows[-1] -match (': '+$width+'x'+$height+' ')
            @{Pass=$matchesSize;Detail="requested native scene=${width}x${height}; last default target: $($rows[-1])"}
        }}
        @{Kind='Script';Name='ordinary driving moved the car';Script={
            param($ctx)
            $speeds=@($ctx.LogLines | ForEach-Object {
                if($_ -match '^\[motion\].*\|v\| ([\d.]+)') {
                    [double]::Parse($Matches[1],[System.Globalization.CultureInfo]::InvariantCulture)
                }
            })
            $moving=@($speeds | Where-Object {$_ -gt 3.0})
            @{Pass=($moving.Count -ge 10);Detail="motion samples=$($speeds.Count), above3m/s=$($moving.Count); visual assessment remains separate"}
        }}
    )
    NativeWidth=$Width
    NativeHeight=$Height
}
if($Capture) {
    #12 consecutive finals and8 aligned pairs only. Source cap requires the
    # shipping DiagDumpPostFxSourcesPC BRN_POSTFX_SOURCE_MAX support.
    $case.DiagEnv+=",BRN_FRAME_DUMP_ARM=0,BRN_FRAME_DUMP_START=$StartPresent,BRN_FRAME_DUMP_MAX=12,BRN_POSTFX_SOURCE_DUMP=1,BRN_POSTFX_SOURCE_START=$StartPresent,BRN_POSTFX_SOURCE_EVERY=1,BRN_POSTFX_SOURCE_MAX=8"
    $case.Checks+=@(
        @{Kind='LogMatch';Name='bounded source observer is live';Pattern='^\[postfx-source\] observation start=\d+ every=1 cap=8$'}
        @{Kind='Script';Name='bounded real scene pairs retain requested resolution';Script={
            param($ctx)
            $pattern='^\[postfx-source\] present=\d+ unit=0 size='+$ctx.Case.NativeWidth+'x'+$ctx.Case.NativeHeight+' .*written=1 '
            $rows=@($ctx.LogLines | Where-Object {$_ -match $pattern})
            @{Pass=($rows.Count -eq 8);Detail="matched native scene pairs=$($rows.Count), requested8 at$($ctx.Case.NativeWidth)x$($ctx.Case.NativeHeight)"}
        }}
        @{Kind='LogCount';Name='bounded real bloom pairs';Pattern='^\[postfx-source\] present=\d+ unit=1 .*written=1 ';Min=8;Max=8}
        @{Kind='Script';Name='final window remains explicitly bounded';Script={
            param($ctx)
            $frames=@(Get-ChildItem -LiteralPath $ctx.FrameDir -Filter 'bb_*.bmp' -File)
            $wrongSize=0
            foreach($frame in $frames) {
                $stream=[IO.File]::OpenRead($frame.FullName)
                try {
                    $header=New-Object byte[] 26
                    if($stream.Read($header,0,26) -ne 26 -or [BitConverter]::ToInt32($header,18) -ne $ctx.Case.NativeWidth -or
                        [Math]::Abs([BitConverter]::ToInt32($header,22)) -ne $ctx.Case.NativeHeight) {++$wrongSize}
                } finally {$stream.Dispose()}
            }
            @{Pass=($frames.Count -eq 12 -and $wrongSize -eq 0);Detail="final images=$($frames.Count), requested12; wrong native dimensions=$wrongSize; inspect movement against presents"}
        }}
    )
}
$case
