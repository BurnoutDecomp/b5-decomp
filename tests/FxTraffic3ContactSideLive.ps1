# FX-TRAFFIC3 (crash parity wave 5, 2026-09-24, CC-2) -- live witness for the traffic contact-side
# detector: TrafficEntityModule::HandleContactPoints @0x827340C0 + ProcessContactPoint @0x82720C68,
# called from PostPhysicsUpdate's RUNNING arm at 0x8274EA68.
#
# The organic Road Rage pursuit of RivalOrganic.ps1 shoves and rear-ends plenty of traffic (the
# FX-FPUMAX run of the same drive logged 200 [T5-slam] and 85 [T5-crash] lines). BRN_TRAFFIC_DIAG
# (already in its DiagEnv) arms three capped lines:
#   [T-contact-pass] contacts=N          a frame whose contact spy carried traffic contacts
#   [T-contact-side] vehicle=V side=S    ProcessContactPoint raised E_CONTACT_SIDE_FRONT / _BACK
#   [T-stuck] vehicle=V front=F back=B   GenerateDriverInputs' wedge arm (0x827496C8) cut the car's
#                                        gas and brake because a stuck timer passed 0.05 s
# Before the fix none of the three could print: the detector had no body, the flags stayed 0 and
# the stuck timers never left 0.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxTraffic3ContactSideLive.ps1 --run-name fxtraffic3_contact_side
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxtraffic3_contact_side'
$case.Area = 'traffic'
$case.Bug = 'A physical traffic car touched at its nose or tail must register the contact side, and while pinned its gas and brake must be cut (HandleContactPoints / ProcessContactPoint, CC-2).'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'context: traffic contacts reached HandleContactPoints'
       Pattern = '\[T-contact-pass\] contacts=[1-9]'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'CC-2 dispatched: ProcessContactPoint raised a nose/tail contact flag'
       Pattern = '\[T-contact-side\] vehicle=\d+ side=(front|back)'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'CC-2 consequence: the wedge arm cut a flagged car''s gas and brake'
       Pattern = '\[T-stuck\] vehicle=\d+ .*-> gas/brake cut'; Min = 1 }
    @{ Kind = 'Script'; Name = 'contact-side geometry is the console''s (|right| <= 0.75 * halfX ; |dir| >= 0.75 * halfZ)'; Script = {
        param($ctx)
        $n = 0; $front = 0; $back = 0; $bad = @()
        $inv = [cultureinfo]::InvariantCulture
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[T-contact-side\] vehicle=(\d+) side=(front|back) dir=([^ ]+) halfZ=([^ ]+) right=([^ ]+) halfX=([^ ]+) flags=(\d+)') {
                $n++
                $dir = [double]::Parse($Matches[3], $inv); $halfZ = [double]::Parse($Matches[4], $inv)
                $right = [double]::Parse($Matches[5], $inv); $halfX = [double]::Parse($Matches[6], $inv)
                if ($Matches[2] -eq 'front') { $front++ } else { $back++ }
                $okSide = [math]::Abs($right) -le 0.75 * $halfX + 1e-4
                $okDir  = if ($Matches[2] -eq 'front') { $dir -ge 0.75 * $halfZ - 1e-4 } else { $dir -le -0.75 * $halfZ + 1e-4 }
                if (-not ($okSide -and $okDir)) { $bad += $line }
            }
        }
        @{ Pass = ($n -gt 0 -and $bad.Count -eq 0); Detail = "$n flag lines ($front front, $back back); outside the console bands: $($bad.Count) $(($bad | Select-Object -First 1))" }
    } }
)
$case
