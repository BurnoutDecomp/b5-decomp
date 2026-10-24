# A real warm junkyard reentry after the initial selection has released its GUI.
# The harness places the car at the authored junkyard 250700 trigger, then uses
# ordinary throttle/accept controls. It does not call EnterJunkyard directly.
@{
    Name = 'junkyard_gui_reload'
    Area = 'gui/resources'
    Bug = 'b5-decomp #53: warm junkyard GUI loads must complete without overflowing model notifications'
    ProfileFixture = 'rival_hunt_profile.sav'
    Frames = $false
    Run = @{
        SkipIntro = $true
        TitleStart = $true
        AcceptGap = 1.0
        MotionProbe = $true
        MaxSeconds = 120
        Teleport = '3007.888916,-2.54,-1956.691040,0'
        MenuScript = "timeout:35;hold:Accelerate:4;wait:=== CarSelectManager: EnterJunkyard:;wait:\[screen\] ENTER 'CS_VEHICLE;tapuntil:Accept:CarSelectManager: Exit state is finished;mark:warm-junkyard-exited;hold:Accelerate:5;mark:driven-after-junkyard"
    }
    DiagEnv = 'BRN_SCREEN_DIAG=1,BRN_DRIVETHRU_DIAG=1'
    Checks = @(
        @{ Kind='LogCount'; Name='no assertions'; Pattern='\[ASSERT(?:\s|\])'; Max=0 }
        @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
        @{ Kind='LogCount'; Name='no resource notification queue overflow'; Pattern='Queue overflow|Queue full'; Max=0 }
        @{ Kind='LogMatch'; Name='ordinary drive-through enters the junkyard'; Pattern='\[drivethru\] ENTER type=0 id=250700' }
        @{ Kind='LogCount'; Name='junkyard GUI bundle loads again'; Pattern="apt bundle 'GuiApt\\BrnCarSelectMain.bundle' -> loaded"; Min=2 }
        @{ Kind='LogCount'; Name='initial and warm junkyard exits finish'; Pattern='CarSelectManager: Exit state is finished'; Min=2 }
        @{ Kind='Script'; Name='warm reentry input sequence completes'; Script={
            param($ctx)
            @{ Pass=($ctx.MarksText -match 'mark warm-junkyard-exited' -and
                     $ctx.MarksText -match 'mark driven-after-junkyard'); Detail='Natural trigger entry, real car selection, exit and final accelerator hold' }
        } }
        @{ Kind='Script'; Name='driving resumes after the warm junkyard exit'; Script={
            param($ctx)
            $exits=@(); $gas=0; $distance=0.0; $previous=$null
            for ($i=0; $i -lt $ctx.LogLines.Count; ++$i) {
                if ($ctx.LogLines[$i] -match 'CarSelectManager: Exit state is finished') { $exits+= $i }
            }
            if ($exits.Count -lt 2) { return @{ Pass=$false; Detail='warm exit not reached' } }
            foreach ($line in $ctx.LogLines[$exits[-1]..($ctx.LogLines.Count-1)]) {
                if ($line -match '^\[motion\].* pos (\S+) (\S+) (\S+) .* gas (\S+)') {
                    $x=[double]::Parse($Matches[1],[cultureinfo]::InvariantCulture)
                    $z=[double]::Parse($Matches[3],[cultureinfo]::InvariantCulture)
                    if ([double]::Parse($Matches[4],[cultureinfo]::InvariantCulture) -gt 0.1) { ++$gas }
                    if ($null -ne $previous) {
                        $dx=$x-$previous[0]; $dz=$z-$previous[1]
                        $step=[math]::Sqrt($dx*$dx+$dz*$dz)
                        if ($step -lt 20) { $distance+=$step }
                    }
                    $previous=@($x,$z)
                }
            }
            @{ Pass=($gas -ge 3 -and $distance -ge 5); Detail="gas samples=$gas distance=$distance m after warm exit" }
        } }
    )
}
