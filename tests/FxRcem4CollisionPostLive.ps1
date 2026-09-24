# FX-RCEM4 (crash parity 2026-09-24, reviewer A on 65eadffe) -- live witness for the shared tail of
# ActiveRaceCar::Update_PreScene @0x822EAE08 (0x822EB0F4..0x822EB158): when a car's collision state or
# culling group changed (AddToCollision / RemoveFromCollision / UpdateCullingGroup set +0x78D / +0x78F),
# the next PreScene posts VehicleInputInterface::SetRaceCarCollision / SetRaceCarCullingGroup and
# clears the flag. BRN_COLLISION_POST_DIAG arms the capped [collision-post] line printed right before
# the posts. Before the fix the flags were left set forever and nothing was posted, so the line could
# not appear. The run must stay free of queue-overflow asserts (the vehicle-side queues hold 10).
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxRcem4CollisionPostLive.ps1 --run-name fxrcem4_collision_post
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxrcem4_collision_post'
$case.Bug = 'ActiveRaceCar::Update_PreScene must post a changed collision state / culling group to the vehicle manager and consume the flags (0x822EB0F4 / 0x822EB128).'
$case.DiagEnv += ',BRN_COLLISION_POST_DIAG=1'
$case.Run.MaxSeconds = 150
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'no event-queue overflow'; Pattern = 'Reached Max length'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'a collision-state change was posted (AddToCollision -> SetRaceCarCollision on)'
       Pattern = '\[collision-post\] slot \d+ entity \d+ collision on '; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'a culling-group change was posted (UpdateCullingGroup -> SetRaceCarCullingGroup)'
       Pattern = '\[collision-post\] slot \d+ entity \d+ collision \S+ culling [0-9]'; Expect = $true }
    @{ Kind = 'Script'; Name = 'each change is posted once (the flags are consumed)'; Script = {
        param($ctx)
        $l = @($ctx.LogLines | Where-Object { $_ -match '\[collision-post\]' })
        $max = ($l | Group-Object { $_.Trim() } | Measure-Object -Property Count -Maximum).Maximum
        @{ Pass = ($l.Count -gt 0 -and $l.Count -lt 64);
           Detail = "$($l.Count) post line(s) (cap 64; a flag left set would post every frame and hit the cap), most repeated line x$max; first: " + $(if ($l.Count) { $l[0].Trim() } else { 'none' }) }
    } }
)
$case
