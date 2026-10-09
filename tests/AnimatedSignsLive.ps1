# Stationary downtown capture. Teleport only selects the scene; animation uses
# the normal renderer time and the material's authored animation constants.
@{
    Name = 'animated_signs_downtown'
    Area = 'graphics'
    Bug = 'Downtown animated displays and neon signs remain on their first frame.'
    Frames = $true
    ProfileFixture = 'rival_hunt_profile.sav'
    Run = @{
        Drive = $true
        DriveDelay = 10
        SkipIntro = $true
        AcceptGap = 1.0
        Teleport = '3040.7,-5.8,-1937.9,180'
        ThrottleScript = '0:accel,3:handbrake'
        MaxSeconds = 75
        FrameEvery = 120
    }
    DiagEnv = 'BRN_MATERIAL_ANIM_DIAG=1,BRN_FRAME_DUMP_MAX=120'
    Checks = @(
        @{ Kind='Mark'; Name='reached driving'; Phase='DRIVING' }
        @{ Kind='NewAsserts'; Name='no new assertions' }
        @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
        @{ Kind='LogCount'; Name='no deferred animation path'; Pattern='animated CPU-constant path deferred'; Max=0 }
        @{ Kind='LogMatch'; Name='authored material animation attached'; Pattern='\[material-anim\] attach ' }
        @{ Kind='LogMatch'; Name='CPU animation calculation runs during mesh dispatch'; Pattern='\[material-anim\] draw ' }
        @{ Kind='Script'; Name='render time advances and authored UV offsets change'; Script={
            param($ctx)
            $values = @{}
            foreach ($line in $ctx.LogLines) {
                if ($line -match '\[material-anim\] draw material=(\d+) time=([\d.]+) u=(-?[\d.]+) v=(-?[\d.]+)') {
                    $key=$Matches[1]
                    if (-not $values.ContainsKey($key)) { $values[$key]=@{} }
                    $values[$key]["$($Matches[3]),$($Matches[4])"]=$true
                }
            }
            $moving=@($values.Keys | Where-Object { $values[$_].Count -ge 2 })
            @{ Pass=($moving.Count -ge 3); Detail="$($moving.Count) distinct materials reached multiple authored animation offsets" }
        } }
    )
}
