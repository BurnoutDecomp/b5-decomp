# Final producer-to-control follow-up after full director-camera latch/up repair.
$case=& (Join-Path $PSScriptRoot 'PlaytestShowtimeControlsLive.ps1')
$case.Name='playtest_showtime_camera_controls'
$case.Bug='The actual complete director camera must reach Showtime aftertouch, including changing lateral axes.'
# The recorded entry/four-axis sequence completes at about82s;35s remain for
# repeated bounces/integration without duplicating the old160-second proof.
$case.Run.MaxSeconds=117
foreach($check in $case.Checks){
    if($check.Name -ne 'original Showtime launch pop'){continue}
    $check.Clear()
    $check.Kind='Script'
    $check.Name='original valid Showtime entry arm'
    $check.Script={
        param($ctx)
        $entry=@($ctx.LogLines | Where-Object {$_ -match '^\[showtime\] enter:'} | Select-Object -First 1)
        if($entry.Count -eq 0){return @{Pass=$false;Detail='no original Showtime entry branch recorded'}}
        if($entry[0] -match 'enter: LAUNCH arm'){
            $pop=@($ctx.LogLines | Where-Object {$_ -match '^\[showtime\] launch pop:'}).Count
            return @{Pass=($pop -gt 0);Detail="original ground-launch branch; pop records=$pop"}
        }
        if($entry[0] -match 'enter: airborne/crash arm \(hasAir=(?<air>[01]) crashing=(?<crash>[01])\) pushT=0\.001'){
            return @{Pass=($Matches.air -eq '1' -or $Matches.crash -eq '1');Detail=$entry[0]+'; original branch skips the ground launch'}
        }
        @{Pass=$false;Detail='unrecognized original entry branch: '+$entry[0]}
    }
}
$case.Checks+=@(
    @{Kind='Script';Name='full delivered camera basis is finite and orthogonal';Script={
        param($ctx)
        $count=0;$bad=0;$maxDot=0.0;$maxLengthError=0.0
        foreach($line in $ctx.LogLines){
            if($line -notmatch '^\[showtime-control-basis\] x (.*?) y (.*?) z (.*?) pos (.*?)$'){continue}
            ++$count;$rows=@()
            foreach($key in @(1,2,3,4)){$rows+=,@($Matches[$key].Split(' ') | ForEach-Object {[double]::Parse($_,[cultureinfo]::InvariantCulture)})}
            if(@($rows | ForEach-Object {$_} | Where-Object {[double]::IsNaN($_) -or [double]::IsInfinity($_)}).Count){++$bad;continue}
            for($a=0;$a -lt 3;++$a){
                for($b=$a;$b -lt 3;++$b){
                    $dot=0.0;for($i=0;$i -lt 3;++$i){$dot+=$rows[$a][$i]*$rows[$b][$i]}
                    if($a -eq $b){$error=[math]::Abs($dot-1.0);$maxLengthError=[math]::Max($maxLengthError,$error)}
                    else{$error=[math]::Abs($dot);$maxDot=[math]::Max($maxDot,$error)}
                    # Original orthogonality tolerance used by the vehicle basis
                    # checks is0.01; no flattening is applied to this observation.
                    if($error -gt 0.01){++$bad}
                }
            }
        }
        @{Pass=($count -ge 6 -and $bad -eq 0);Detail="full bases=$count, mismatches=$bad, max dot=$maxDot, max squared-length error=$maxLengthError"}
    }}
    @{Kind='Script';Name='lateral control axis follows the rotated camera';Script={
        param($ctx)
        $count=0;$rotated=0;$maxZ=0.0
        foreach($line in $ctx.LogLines){
            if($line -notmatch '^\[showtime-control\].* cameraX (?<x>\S+) \S+ (?<z>\S+) cameraZ '){continue}
            ++$count;$x=[double]::Parse($Matches.x,[cultureinfo]::InvariantCulture);$z=[double]::Parse($Matches.z,[cultureinfo]::InvariantCulture)
            $maxZ=[math]::Max($maxZ,[math]::Abs($z))
            if([math]::Abs($z) -gt 0.1 -and [math]::Abs($x-1.0) -gt 0.01){++$rotated}
        }
        @{Pass=($count -ge 6 -and $rotated -ge 2);Detail="observations=$count, rotated lateral axes=$rotated, max abs cameraX.z=$maxZ"}
    }}
)
$case
