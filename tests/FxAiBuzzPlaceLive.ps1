# FX-AIBUZZ item 1 (crash parity 2026-09-24) live witness: free-roam buzz-by placement.
#
# RaceCarEntityModule::PlaceRaceCarOnLoad @0x822CE588, ARM A1 (0x822CE780..0x822CE7FC): a free-roam rival
# (not in a game mode, racing start state) whose resources finish loading within 250 m of the player is
# handed to BrnAI::BuzzBy::MaintainAheadOrBehind @0x82766C40 and reset with the request it returns --
# AHEAD of the player (Dot > 0): type 5 (same heading) / 4 (facing), speed flt_8300DBEC 35.7632, 200 m;
# otherwise type 3, speed |player velocity| + flt_8300D7F4 11.176, -60 m. Before the fix the leaf had no
# body and the arm was a named park (`[ai-attach] ... arm A1-buzzBy-PARKED`): the rival was never placed.
#
# The rivals come from the PROFILE (ProgressionManager::UpdateRivals posts every UNLOCKED/FLEEING rival at
# junkyard exit, BRN_PROGRESSION_RIVALS prints the counts) and reach the player through BuzzBy::Update's
# buzz timer (10..180 s of buzzable free-roam driving), so the drive is the free-roam road recipe that
# produced the four PARKED hits of 2026-09-22/23 (fxvehphys_drift 20260923_144455 etc.): a long gas-held
# run from the teleport south-east of the junkyard. Wall-clock coupled -- a run where no rival comes into
# range proves nothing either way (read the [rivals] line first: unlocked=0 means the profile has none).
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxAiBuzzPlaceLive.ps1
@{
    Name = 'fxaibuzz_place'
    Area = 'ai/buzzby'
    Bug = 'Free-roam buzz-by placement: PlaceRaceCarOnLoad ARM A1 must run BuzzBy::MaintainAheadOrBehind and reset the rival (was a named park).'
    Frames = $false
    Run = @{
        Drive = $true
        MotionProbe = $true
        SkipIntro = $true
        AcceptGap = 1.0
        Teleport = '2641.5,1.3,-1723.8,169'
        SkipTrainingTip = $true
        MaxSeconds = 170
    }
    DiagEnv = 'BRN_BUZZBY_PLACE_DIAG=1 BRN_PROGRESSION_RIVALS=1'
    Checks = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
        @{ Kind = 'LogMatch'; Name = 'the profile posted its free-roam rivals (UpdateRivals witness)'
           Pattern = '\[rivals\] update: profileRivals=\d+ unlocked=\d+ posted=[1-9]\d*'; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'ARM A1 dispatched MaintainAheadOrBehind ([buzzby-place], BRN_BUZZBY_PLACE_DIAG)'
           Pattern = '\[buzzby-place\] MaintainAheadOrBehind global \d+ .* -> resetType [345] speed '; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'the [ai-attach] arm is A1-buzzBy with a real reset type'
           Pattern = '\[ai-attach\] PlaceRaceCarOnLoad global \d+ type \d+ opponent -?\d+ arm A1-buzzBy resetType [345] '; Expect = $true }
        @{ Kind = 'LogCount'; Name = 'the named park is gone'; Pattern = 'A1-buzzBy-PARKED'; Max = 0 }
        # The console's answer, re-derived from the witness's own inputs on every placement:
        #   along > 0  -> type 5 (headingDot > 0) or 4, speed 35.7632 (flt_8300DBEC), resetDist 200
        #   along <= 0 -> type 3, speed playerSpeed + 11.176 (flt_8300D7F4), resetDist -60
        @{ Kind = 'Script'; Name = 'every placement is the console arithmetic on its own inputs'; Script = {
            param($ctx)
            $re = '\[buzzby-place\] MaintainAheadOrBehind global (?<g>\d+) dist (?<d>\S+) along (?<a>\S+) headingDot (?<h>\S+) playerSpeed (?<p>\S+) -> resetType (?<t>\d+) speed (?<s>\S+) resetDist (?<r>\S+)'
            $lines = @($ctx.LogLines | Where-Object { $_ -match $re })
            if ($lines.Count -eq 0) { return @{ Pass = $false; Detail = 'no [buzzby-place] line (the arm never ran)' } }
            $inv = [System.Globalization.CultureInfo]::InvariantCulture
            $bad = @()
            foreach ($l in $lines) {
                if ($l -notmatch $re) { continue }
                $a = [double]::Parse($Matches.a, $inv); $h = [double]::Parse($Matches.h, $inv)
                $p = [double]::Parse($Matches.p, $inv); $t = [int]$Matches.t
                $s = [double]::Parse($Matches.s, $inv); $r = [double]::Parse($Matches.r, $inv)
                if ($a -gt 0) { $wantT = $(if ($h -gt 0) { 5 } else { 4 }); $wantS = 35.7632; $wantR = 200.0 }
                else          { $wantT = 3; $wantS = $p + 11.176; $wantR = -60.0 }
                if ($t -ne $wantT -or [math]::Abs($s - $wantS) -gt 0.01 -or $r -ne $wantR) { $bad += $l.Trim() }
            }
            return @{ Pass = ($bad.Count -eq 0)
                      Detail = ('{0} placement(s), {1} off the console arithmetic{2}' -f $lines.Count, $bad.Count,
                                $(if ($bad.Count) { '; first: ' + $bad[0] } else { '; first: ' + $lines[0].Trim() })) }
          } }
        # The request is serviced: a non-player slot goes ACTIVE after the first placement (the reset pump ->
        # PlaceOnTrackManager -> ResetActiveRaceCar; the PARKED arm left the car in E_STATE_WAITING).
        @{ Kind = 'Script'; Name = 'a rival slot goes E_STATE_ACTIVE after the buzz-by placement'; Script = {
            param($ctx)
            $first = -1
            for ($i = 0; $i -lt $ctx.LogLines.Count; $i++) {
                if ($ctx.LogLines[$i] -match '\[buzzby-place\] MaintainAheadOrBehind') { $first = $i; break }
            }
            if ($first -lt 0) { return @{ Pass = $false; Detail = 'no [buzzby-place] line' } }
            for ($i = $first; $i -lt $ctx.LogLines.Count; $i++) {
                if ($ctx.LogLines[$i] -match '\[PLACEONTRACK\] race car [1-7] -> E_STATE_ACTIVE') {
                    return @{ Pass = $true; Detail = ('line {0}: {1}' -f ($i + 1), $ctx.LogLines[$i].Trim()) }
                }
            }
            return @{ Pass = $false; Detail = 'no rival slot reached E_STATE_ACTIVE after the placement' }
          } }
    )
}
