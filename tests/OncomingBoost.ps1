@{
    Name='oncoming_boost'; Area='boost'; Bug='Wrong-way lane tags activate oncoming boost earning.'
    Frames=$true
    Run=@{Drive=$true; SkipIntro=$true; Teleport='3040.7,-5.8,-1937.9,0'; MaxSeconds=45; AcceptGap=1; FrameEvery=12; MinFreeGB=2}
    DiagEnv='BRN_ONCOMING_DIAG=1'
    Checks=@(
        @{Kind='NewAsserts';Name='no new asserts'}
        @{Kind='LogCount';Name='no exceptions';Pattern='\[EXCEPTION\]';Max=0}
        @{Kind='Mark';Name='reached driving';Phase='DRIVING'}
        @{Kind='Script';Name='oncoming road tags produce rising boost';Script={
            param($ctx)
            $previous=$null; $rises=0; $samples=0
            foreach($line in $ctx.LogLines) {
                if ($line -match '\[oncoming\].*traffic=0 .*state=0 active=1 speed=([\d.]+) boost=([\d.]+)') {
                    $boost=[double]::Parse($Matches[2],[cultureinfo]::InvariantCulture)
                    if ($null -ne $previous -and $boost -gt $previous) { $rises++ }
                    $previous=$boost; $samples++
                } elseif ($line -match '\[oncoming\]') { $previous=$null }
            }
            @{Pass=($rises -ge 2);Detail="$samples valid oncoming samples, $rises consecutive boost increases"}
        }}
    )
}
