# FX-GEOMETRIC (crash parity 2026-09-24) -- live witness for the place-on-track ROUND TRIP after a crash:
#   RaceCarEntityModule::PostSceneUpdate (0x822FE598) -> PlaceOnTrackManager::PostSceneUpdate @0x822D3168
#   posts a 100 m vertical world-only fine line test per pending request ("Generating line test ...",
#   "Line test requested") -> SceneManagerModule::ProcessLineTestFine -> CollideLineAgainstPolySoupList
#   (PolygonSoupListSpatialMap::RunQuery(const Line&) @0x82843E98, IntersectLinePolygonSoupSingleSided
#   @0x8283C598) answers it the same frame -> PlaceOnTrackManager::PrePhysicsUpdate walks the answer
#   ("Received line test result ...", "miNumIntersections=", "Selected intersection ...") and places the
#   car ("Place on track request complete"). All of these are the CONSOLE's own prints
#   (gxMessageFilterFlags & 1), not a PC diag.
# Before the fix nothing produced the query; every request was answered by the PC-only WORLDCOL.BIN walk
# ApplyPendingRequestsWithoutSceneQueryBringUp ("[PLACEONTRACK] [FLAG PC bring-up] no scene fine-query round
# trip; ..."), retired in the same change -- with both live every car would have been placed twice.
# The recipe is the deterministic crash-sweep cell h225_s70 (crash_sweep_batch.ps1 -Headings 225 -Speeds 70
# -MinDamageableSeconds 1.6): launch 112 m out, a 70 m/s wall crash, the crash-complete reset. Pre-fix
# reference for the same cell (scratch/flow_run/hbrigid_h225_s70_r1, WORLDCOL walk): junkyard -3.525000,
# launch (3249.795898, -1925.404053) -> -3.884746, crash reset (3167.192627, -1998.358154) -> -4.078719.
# First run with the round trip (scratch/flow_run/hbgeom_h225_s70_r1): -3.525002 / -3.884750 / -4.078720,
# identical normals.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxGeometricPlaceOnTrackLive.ps1
@{
    Name   = 'fxgeometric_place_on_track'
    Area   = 'racecar/place-on-track'
    Bug    = 'A crash reset must be placed by the console round trip (PostSceneUpdate query -> scene fine line test -> PrePhysicsUpdate walk), exactly once per request, with no PC WORLDCOL.BIN answer (FX-GEOMETRIC).'
    Frames = $false
    Run    = @{
        Drive           = $true
        CrashSweep      = '3249.796,-3.7,-1925.404'
        CrashSweepShots = '225:70'
        CrashSweepArm   = 4
        MaxSeconds      = 75
    }
    DiagEnv = 'BRN_CRASH_RESPONSE_DIAG=1'
    Checks  = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached DRIVING'; Phase = 'DRIVING' }
        @{ Kind = 'LogCount'; Name = 'the sweep fired and seated the car on road'; Pattern = '\[sweep\] seat ok shot 0 '; Min = 1 }
        @{ Kind = 'LogCount'; Name = 'the producer posted line tests (PostSceneUpdate @0x822D3168 dispatched)'
           Pattern = '\[PLACEONTRACK\] Generating line test to place race car \d+ on track'; Min = 3 }
        @{ Kind = 'LogCount'; Name = 'the retired PC WORLDCOL.BIN answer never runs'
           Pattern = 'no scene fine-query round trip'; Max = 0 }
        @{ Kind = 'Script'; Name = 'every request is answered and placed exactly once (produced -> answered -> consumed)'; Script = {
            param($ctx)
            $state = @{}; $bad = @(); $counts = @{ gen = 0; recv = 0; done = 0 }; $open = $null
            foreach ($l in $ctx.LogLines) {
                if ($l -match '\[PLACEONTRACK\] Generating line test to place race car (\d+) on track') {
                    $counts.gen++; $s = [int]$Matches[1]
                    if ($state[$s] -eq 'asked') { $bad += "slot $s asked twice before an answer" }
                    $state[$s] = 'asked'; continue }
                if ($l -match '\[PLACEONTRACK\] Received line test result for race car (\d+)') {
                    $counts.recv++; $s = [int]$Matches[1]
                    if ($state[$s] -ne 'asked') { $bad += "answer for slot $s with no open request" }
                    $state[$s] = 'answered'; $open = $s; continue }
                if ($l -match '\[PLACEONTRACK\] Place on track request complete') {
                    $counts.done++
                    if ($null -eq $open) { $bad += 'a placement with no answer (a second placement?)' }
                    elseif ($state[$open] -ne 'answered') { $bad += "second placement for slot $open" }
                    else { $state[$open] = 'placed' }
                    $open = $null; continue }
            }
            $pending = @($state.Keys | Where-Object { $state[$_] -ne 'placed' })
            @{ Pass = ($counts.gen -gt 0 -and $bad.Count -eq 0 -and $counts.gen -eq $counts.recv -and $counts.recv -eq $counts.done -and $pending.Count -eq 0);
               Detail = "requests $($counts.gen), answers $($counts.recv), placements $($counts.done)" + $(if ($bad.Count) { '; ' + ($bad -join ' | ') } else { '' }) + $(if ($pending.Count) { "; open: $($pending -join ',')" } else { '' }) }
        } }
        @{ Kind = 'Script'; Name = 'the crash-complete reset is placed by the round trip'; Script = {
            param($ctx)
            $phase = 0; $done = 0; $detail = @()
            foreach ($l in $ctx.LogLines) {
                if ($l -match '\[crash-exit\] CRASH COMPLETE posted for active race car 0 ') { $phase = 1; continue }
                if ($phase -eq 1 -and $l -match '\[resetpump\] RESULT applied: global car 0 SUCCESS \(AI pose\) -> \(([^)]*)\)') { $phase = 2; $pose = $Matches[1]; continue }
                if ($phase -eq 2 -and $l -match '\[PLACEONTRACK\] Generating line test to place race car 0 on track') { $phase = 3; continue }
                if ($phase -eq 3 -and $l -match '\[PLACEONTRACK\] Received line test result for race car 0') { $phase = 4; continue }
                if ($phase -eq 4 -and $l -match 'Selected reset data: lResetPosition=\(([^)]*)\)') { $seat = $Matches[1]; continue }
                if ($phase -eq 4 -and $l -match '\[PLACEONTRACK\] Place on track request complete') { $done++; $detail += "AI pose ($pose) -> seated ($seat)"; $phase = 0; continue }
            }
            @{ Pass = ($done -ge 1); Detail = "$done crash reset(s) through the round trip: " + ($detail -join ' | ') }
        } }
        @{ Kind = 'Script'; Name = 'INFO -- hits per answer and empty answers reverted to the ring (never fails)'; Script = {
            param($ctx)
            $hist = @{}; $parks = 0
            foreach ($l in $ctx.LogLines) {
                if ($l -match 'lpLineTestResult->miNumIntersections=(-?\d+)') { $hist[$Matches[1]] += 1 }
                if ($l -match 'Failed to find valid place on track location') { $parks++ }
            }
            $h = ($hist.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name) hit(s) x$($_.Value)" }) -join ', '
            @{ Pass = $true; Detail = "$h; empty answers reverted to GetResetCoords $parks" }
        } }
    )
}
