# FX-WEDGEVEL (crash parity 2026-09-24) -- live witness for THE WEDGED-CAR SIGNATURE: the player car held
# against geometry with the throttle on keeps a non-zero RaceCarState::mLinearVelocity while its position
# does not move. Banked organic occurrences (both with -Drive held, both at y ~ -8.4..-8.7):
#   scratch/bugtest/runs/fxgeometric/20260924_164556   [motion] n 5880..10470 pinned at (2208.7146,-8.3752,-2369.7752),
#                                                      |v| 0.955 m/s, heading (-0.934,0.025,-0.356)
#   scratch/bugtest/runs/rcem_takedown_flow/20260922_214835  n 4200..5190 pinned at (2558.2,-8.668,-2315.39),
#                                                      |v| ~0.9 m/s, heading (-0.04,0.028,0.999)
# The recipe replays both sites DETERMINISTICALLY with the frame-counted crash sweep. Shot 0 is a parking shot
# (speed 0, where the junkyard exit already put the car) that only lets the harness throttle come on; shots 1
# and 2 are each ONE RequestPlaceOnTrack 6 m behind a banked wedge pose, on the wedge heading, at 3 m/s, with
# the throttle already held, then 600 sim frames. BRN_WEDGE_PROBE (ExternalPhysicsBody.cpp, NOT IN THE X360
# BINARY) opens a 240-step window when the watched (player) body shows the signature for 30 steps and prints,
# per step, the [dv] ledger (velocity + position at every stage boundary of PhysicsModule::Update and every
# CalculateNewVelocity drain), the world-impulse arm ([wedge-imp]), the contact census ([wedge-contacts]) and
# the penetration solver's push-out ([wedge-pen]).
#
# VERDICT (fixes/FX-WEDGEVEL.md): CONSOLE BEHAVIOUR. The residual velocity is the console's own balance --
# the tyres add ~0.18 m/s per step into the wall, the world contacts take back the same through the console's
# deliberately soft velocity response (ApplyCarWorldImpulse @0x82624898 x0.5, the sensor's 0.2 absorption cap
# while not crashing, ApplyWallContactImpulse @0x825FEA18 x0.25 => 10% of the inelastic impulse per contact),
# and DeformationManager::SolvePenetration @0x82621B08 pins the pose with a POSITION-ONLY push-out (phase 3
# stores the transform rows only). So the checks below are a GUARD on that console chain, not a bug test:
# they fail if a later change removes velocity at the push-out, changes the pass-on scale, or adds a
# non-drain velocity writer to the step.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxWedgevelWedgeLive.ps1
@{
    Name   = 'fxwedgevel_wedge'
    Area   = 'physics/contact'
    Bug    = 'A player car pushed into geometry with the throttle held keeps ~0.9 m/s of mLinearVelocity while its pose is pinned: prove where it survives and that every leg is the console (FX-WEDGEVEL).'
    Frames = $false
    Run    = @{
        Drive            = $true
        MotionProbe      = $true
        SkipIntro        = $true
        CrashSweep       = '3007.972,-2.54,-1945.167'
        CrashSweepShots  = '3007.972/-2.54/-1945.167/0:0,2214.321/-8.4/-2367.638/249.11:3,2558.476/-8.7/-2321.390/357.65:3'
        CrashSweepSettle = 600
        CrashSweepMax    = 600
        CrashSweepArm    = 4
        MaxSeconds       = 105
    }
    DiagEnv = 'BRN_WEDGE_PROBE=240'
    Checks  = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached DRIVING'; Phase = 'DRIVING' }
        @{ Kind = 'LogCount'; Name = 'the wedge probe armed (control: a silent run is not "no wedge")'; Pattern = '\[wedge\] ARMED'; Min = 1 }
        @{ Kind = 'LogCount'; Name = 'all three shots seated on road'; Pattern = '\[sweep\] seat ok shot [012] '; Min = 3 }
        @{ Kind = 'LogCount'; Name = 'no SEAT BAD shot'; Pattern = '\[sweep\] SEAT BAD'; Max = 0 }
        @{ Kind = 'LogCount'; Name = 'the throttle reached the car'; Pattern = '\[motion\] .* gas 1\.0'; Min = 10 }
        @{ Kind = 'LogCount'; Name = 'both banked wedge sites wedge again (one [wedge] window each)'; Pattern = '^\[wedge\] OPEN window'; Min = 2 }
        @{ Kind = 'Script'; Name = 'the push-out is POSITION-ONLY: @postphys moves the pose and never the velocity (SolvePenetration @0x82621B08 phase 3)'; Script = {
            param($ctx)
            $steps = 0; $moved = 0; $bad = 0; $inWin = $false
            foreach ($l in $ctx.LogLines) {
                if ($l -match '^\[dv\] STEP \d+ f -?\d+ wedge 1 ') { $inWin = $true; continue }
                if ($l -match '^\[dv\] STEP ') { $inWin = $false; continue }
                if ($inWin -and $l -match '^\[dv\]   mark   @postphys v \S+ \S+ \S+ segDv (\S+) (\S+) (\S+) \|segDv\| \S+ p \S+ \S+ \S+ segDp (\S+) (\S+) (\S+)') {
                    $steps++
                    $dv = [math]::Abs([double]$Matches[1]) + [math]::Abs([double]$Matches[2]) + [math]::Abs([double]$Matches[3])
                    $dp = [math]::Abs([double]$Matches[4]) + [math]::Abs([double]$Matches[5]) + [math]::Abs([double]$Matches[6])
                    if ($dv -gt 1e-6) { $bad++ }
                    if ($dp -gt 1e-4) { $moved++ }
                }
            }
            @{ Pass = ($steps -ge 100 -and $bad -eq 0 -and $moved -ge [int]($steps * 0.9)); Detail = "$steps window steps: push-out moved the pose on $moved, changed the velocity on $bad" }
        } }
        @{ Kind = 'Script'; Name = 'every world impulse reaching the body is 0.2x the shaped one (0.8 pass-on x0.25 ApplyWallContactImpulse @0x825FEA18)'; Script = {
            param($ctx)
            $imps = @{}; $drains = @{}; $cur = -1
            foreach ($l in $ctx.LogLines) {
                if ($l -match '^\[wedge-imp\] step (\d+) sensor \d+ .* shaped (\S+) dir') { $s = [int]$Matches[1]; if (-not $imps.ContainsKey($s)) { $imps[$s] = @() }; $imps[$s] += [double]$Matches[2]; continue }
                if ($l -match '^\[dv\] STEP (\d+) f -?\d+ wedge 1 ') { $cur = [int]$Matches[1]; continue }
                if ($l -match '^\[dv\] STEP ') { $cur = -1; continue }
                if ($cur -ge 0 -and $l -match '^\[dv\]   drain  @\S+ site world rva \S+ ra \S+ J (\S+) (\S+) (\S+) Fdt') {
                    if (-not $drains.ContainsKey($cur)) { $drains[$cur] = @() }
                    $jx = [double]$Matches[1]; $jy = [double]$Matches[2]; $jz = [double]$Matches[3]
                    $drains[$cur] += [math]::Sqrt($jx*$jx + $jy*$jy + $jz*$jz) }
            }
            $pairs = 0; $bad = 0; $mism = 0; $lo = 9.0; $hi = 0.0
            foreach ($s in $imps.Keys) {
                if (-not $drains.ContainsKey($s) -or $drains[$s].Count -ne $imps[$s].Count) { $mism++; continue }
                for ($i = 0; $i -lt $imps[$s].Count; $i++) {
                    if ($imps[$s][$i] -le 0) { continue }
                    $r = $drains[$s][$i] / $imps[$s][$i]; $pairs++
                    if ($r -lt $lo) { $lo = $r }; if ($r -gt $hi) { $hi = $r }
                    if ($r -lt 0.198 -or $r -gt 0.2005) { $bad++ } }
            }
            @{ Pass = ($pairs -ge 100 -and $bad -eq 0 -and $mism -eq 0); Detail = ("{0} contact/drain pairs, ratio [{1:f5}, {2:f5}], {3} outside [0.198, 0.2005], {4} steps with unpaired lines" -f $pairs, $lo, $hi, $bad, $mism) }
        } }
        @{ Kind = 'Script'; Name = 'the only velocity change outside a drain is gravity at @integrate (ReadUpdatedBodies @0x82619A10: v.y -= 9.81*dt)'; Script = {
            param($ctx)
            $steps = 0; $bad = 0; $inWin = $false; $dt = 0.0
            foreach ($l in $ctx.LogLines) {
                if ($l -match '^\[dv\] STEP \d+ f -?\d+ wedge 1 dt (\S+) ') { $inWin = $true; $dt = [double]$Matches[1]; $steps++; continue }
                if ($l -match '^\[dv\] STEP ') { $inWin = $false; continue }
                if ($inWin -and $l -match '^\[dv\]   mark   @(\S+) v \S+ \S+ \S+ segDv (\S+) (\S+) (\S+) ') {
                    $x = [double]$Matches[2]; $y = [double]$Matches[3]; $z = [double]$Matches[4]
                    if ($Matches[1] -eq 'integrate') { $ey = -9.81 * $dt; if ([math]::Abs($x) -gt 1e-6 -or [math]::Abs($z) -gt 1e-6 -or [math]::Abs($y - $ey) -gt 1e-4) { $bad++ } }
                    elseif ([math]::Abs($x) + [math]::Abs($y) + [math]::Abs($z) -gt 1e-6) { $bad++ } }
            }
            @{ Pass = ($steps -ge 100 -and $bad -eq 0); Detail = "$steps window steps, $bad non-drain velocity changes other than gravity" }
        } }
        @{ Kind = 'Script'; Name = 'INFO -- per [wedge] window: mean |v| at @end, mean |dp| per step, the integrate step vs the push-out (never fails)'; Script = {
            param($ctx)
            $win = 0; $rows = @{}; $out = @()
            foreach ($l in $ctx.LogLines) {
                if ($l -match '^\[wedge\] OPEN window (\d+)') { $win = [int]$Matches[1]; $rows[$win] = @{ n = 0; v = 0.0; dp = 0.0; integ = 0.0; post = 0.0; pen = 0 }; continue }
                if ($win -gt 0 -and $l -match '^\[dv\] STEP \d+ f -?\d+ wedge 1 dt (\S+) \|dv\| \S+ v0 \S+ \S+ \S+ v1 (\S+) (\S+) (\S+) p0 (\S+) (\S+) (\S+) p1 (\S+) (\S+) (\S+)') {
                    $r = $rows[$win]; $r.n++
                    $vx = [double]$Matches[2]; $vy = [double]$Matches[3]; $vz = [double]$Matches[4]
                    $r.v += [math]::Sqrt($vx*$vx + $vy*$vy + $vz*$vz)
                    $dx = [double]$Matches[8] - [double]$Matches[5]; $dy = [double]$Matches[9] - [double]$Matches[6]; $dz = [double]$Matches[10] - [double]$Matches[7]
                    $r.dp += [math]::Sqrt($dx*$dx + $dy*$dy + $dz*$dz); continue }
                if ($win -gt 0 -and $l -match '^\[dv\]   mark   @integrate .* segDp (\S+) (\S+) (\S+)') {
                    $r = $rows[$win]; $a = [double]$Matches[1]; $b = [double]$Matches[2]; $c = [double]$Matches[3]; $r.integ += [math]::Sqrt($a*$a + $b*$b + $c*$c); continue }
                if ($win -gt 0 -and $l -match '^\[dv\]   mark   @postphys .* segDp (\S+) (\S+) (\S+)') {
                    $r = $rows[$win]; $a = [double]$Matches[1]; $b = [double]$Matches[2]; $c = [double]$Matches[3]; $r.post += [math]::Sqrt($a*$a + $b*$b + $c*$c); continue }
                if ($win -gt 0 -and $l -match '^\[wedge-pen\] step') { $rows[$win].pen++ }
            }
            foreach ($k in ($rows.Keys | Sort-Object)) {
                $r = $rows[$k]; if ($r.n -eq 0) { continue }
                $out += ("window {0}: {1} steps, mean |v| {2:f3} m/s, mean |dp| {3:f5} m/step, integrate moved {4:f5} m/step, push-out moved {5:f5} m/step, [wedge-pen] {6}" -f $k, $r.n, ($r.v / $r.n), ($r.dp / $r.n), ($r.integ / $r.n), ($r.post / $r.n), $r.pen)
            }
            @{ Pass = $true; Detail = $(if ($out.Count) { $out -join ' | ' } else { 'no [wedge] window opened' }) }
        } }
    )
}
