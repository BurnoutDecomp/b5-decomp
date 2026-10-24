# Warm menu reloads after the real junkyard entry/exit, on a copied profile.
$steps = @('timeout:30', 'sleep:3')
for ($i = 0; $i -lt 12; ++$i) {
    if (($i % 2) -eq 0) {
        $steps += @('tap:PauseMap', "wait:\[screen\] ENTER 'CN_MAP_MAIN", 'sleep:2')
    } else {
        $steps += @('tap:Start', "wait:\[screen\] ENTER 'CN_D_DETAIL", 'sleep:2')
    }
    $steps += @("tapuntil:Stop:\[screen\] ENTER 'INGAME", 'sleep:1')
}
$steps += @('mark:resource-cycles-complete', 'hold:Accelerate:5', 'mark:driven-after-reloads')
@{
    Name = 'gui_resource_reload'
    Area = 'gui/resources'
    Bug = 'b5-decomp #52/#53: warm GUI bundle registrations must not overflow model completion storage'
    ProfileFixture = 'rival_hunt_profile.sav'
    Frames = $false
    Run = @{
        SkipIntro = $true
        TitleStart = $true
        AcceptGap = 1.0
        MotionProbe = $true
        MaxSeconds = 150
        MenuScript = $steps -join ';'
    }
    DiagEnv = 'BRN_SCREEN_DIAG=1,BRN_SATNAV_DIAG=1,BRN_CRASHCAM_DIAG=1'
    Checks = @(
        @{ Kind='LogCount'; Name='no assertions'; Pattern='\[ASSERT(?:\s|\])'; Max=0 }
        @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
        @{ Kind='LogCount'; Name='no resource completion queue overflow'; Pattern='Queue overflow|Queue full'; Max=0 }
        @{ Kind='LogMatch'; Name='real junkyard car selection loaded'; Pattern="apt bundle 'GuiApt\\BrnCarSelectMain.bundle' -> loaded" }
        @{ Kind='LogMatch'; Name='real junkyard exit finished'; Pattern='CarSelectManager: Exit state is finished' }
        @{ Kind='LogCount'; Name='six map entries'; Pattern="\[screen\] ENTER 'CN_MAP_MAIN"; Min=6 }
        @{ Kind='LogCount'; Name='six driver-details entries'; Pattern="\[screen\] ENTER 'CN_D_DETAIL"; Min=6 }
        @{ Kind='Script'; Name='all reload inputs and final driving hold completed'; Script={
            param($ctx)
            @{ Pass=($ctx.MarksText -match 'mark resource-cycles-complete' -and
                     $ctx.MarksText -match 'mark driven-after-reloads'); Detail='12 alternating menu round trips and final accelerator hold' }
        } }
        @{ Kind='Script'; Name='player input and motion resume after the final menu exit'; Script={
            param($ctx)
            $lastExit = -1; $entries = 0
            for ($i = 0; $i -lt $ctx.LogLines.Count; ++$i) {
                if ($ctx.LogLines[$i] -match "\[screen\] ENTER 'CN_(MAP_MAIN|D_DETAIL)") { ++$entries }
                if ($entries -ge 12 -and $ctx.LogLines[$i] -match "\[screen\] ENTER 'INGAME") { $lastExit = $i }
            }
            if ($lastExit -lt 0) { return @{ Pass=$false; Detail='final menu exit not reached' } }
            $gas = 0; $distance = 0.0; $previous = $null
            foreach ($line in $ctx.LogLines[$lastExit..($ctx.LogLines.Count-1)]) {
                if ($line -match '^\[motion\].* pos (\S+) (\S+) (\S+) .* gas (\S+)') {
                    $x = [double]::Parse($Matches[1], [cultureinfo]::InvariantCulture)
                    $z = [double]::Parse($Matches[3], [cultureinfo]::InvariantCulture)
                    if ([double]::Parse($Matches[4], [cultureinfo]::InvariantCulture) -gt 0.1) { ++$gas }
                    if ($null -ne $previous) {
                        $dx=$x-$previous[0]; $dz=$z-$previous[1]
                        $step=[math]::Sqrt($dx*$dx+$dz*$dz)
                        if ($step -lt 20) { $distance += $step }
                    }
                    $previous=@($x,$z)
                }
            }
            @{ Pass=($gas -ge 3 -and $distance -ge 5.0); Detail="gas samples=$gas distance=$distance m after 12 exits" }
        } }
    )
}
