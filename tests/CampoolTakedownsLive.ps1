# L1 CAMPOOL live case (owner's list 2026-09-27, "There are still some asserts and crashes").
#
# The owner's repeated crash: "Ran out of slots when trying to allocate a large behaviour" (BrnBehaviourManager.h),
# then an access violation in ObjectPool<Vector4[250],8,int>::operator[] under NewBehaviour<BehaviourGyroCam> <-
# B3ClassicTakedownPlayer::Prepare <- ArbStateTakedown::Prepare. Two defects filled the eight LARGE camera-behaviour
# slots (the three dumps on exe d65db9997047 all hold the same set):
#   * every BehaviourGyroCam went to the LARGE pool: the PC chose the pool from the host sizeof (1632) against the
#     console's 1600-byte small bucket; the console's AllocateBehaviour<GyroCam> @0x822634D8 reads the SMALL pool
#     (tests/run_campool_behaviour_pools.py);
#   * ArbStateCarSelect had no Release override (@0x82236050), so the junkyard's two IceAnim takes stayed in the
#     large pool for the rest of the session (tests/run_campool_carselect_release.py).
# This case is the organic take-down drive (RivalOrganic: an event started at the teleport, the game's own AI on the
# pad with --ai-pad pursuit ramming the rivals) run long, with the BRN_CAMPOOL_DIAG witness: one line per camera
# behaviour brought up ('+') or handed back ('-') with its size, its pool, both pools' free counts and low-water
# marks, and what the LARGE pool would hold on the code before the fix for the same live set ("old large free").
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/CampoolTakedownsLive.ps1 --ai-pad pursuit --no-frames --slot 1 --run-name campool_takedowns
# RED run: the same case on the pre-fix exe (7fdddf649d45, b5 08be7e89) staged in the slot -- the witness does not
# exist there, so the RED evidence is the assert itself or the missing witness.
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'campool_takedowns'
$case.Area = 'camera'
$case.Bug = 'Many take-downs in one session must never run the large camera-behaviour pool dry (the owner''s crash).'
$case.Frames = $false
# The RETURNING-player boot, as RivalOrganic's take-down runs on slot 0 (junkyard, car select, roaming, then the event
# at the teleport): the first-boot path starts the event with no rival the pursuit can target (measured: `target -1`
# for a whole 300 s run). For an A/B pair on a slot, seed the slot's Memcard_<n>\Profile.sav from the SAME copy of
# slot 0's harness save before each run.
$case.Run.MaxSeconds = 300
$case.DiagEnv += ',BRN_CAMPOOL_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no "Ran out of slots" behaviour-pool assert'; Pattern = 'Ran out of slots when trying to allocate'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'take-down camera states entered (need 2+)'
       Pattern = '\[crashcam\] container current state -> 3\b'; Min = 2 }
    @{ Kind = 'LogMatch'; Name = 'the witness ran: a GyroCam (1632 bytes here) was brought up in the SMALL pool'
       Pattern = '\[campool\] \+ .* size 1632 pool SMALL'; Expect = $true }
    @{ Kind = 'LogCount'; Name = 'no GyroCam in the LARGE pool (the console''s four large types only)'
       Pattern = '\[campool\] \+ .* size 1632 pool LARGE'; Max = 0 }
    @{ Kind = 'LogMatch'; Name = 'the car select handed its behaviours back at the junkyard exit (ArbStateCarSelect::Release)'
       Pattern = '\[campool\] - ArbStateCarSelect::'; Expect = $true }
    @{ Kind = 'Script'; Name = 'the LARGE pool never ran dry (low-water mark of its free count >= 1)'; Script = {
        param($ctx)
        $low = $null; $n = 0
        foreach ($line in $ctx.LogLines) {
            # Anchored on "| large free ... small free": the line also ends with "| old large free", the pre-fix
            # counterfactual, which a greedy `.* large free` would read instead of the real pool.
            if ($line -match '\[campool\] [+-] .*\| large free (-?\d+) \(low (-?\d+)\) small free') {
                $n++; $v = [int]$Matches[2]; if ($null -eq $low -or $v -lt $low) { $low = $v }
            }
        }
        @{ Pass = ($n -gt 0 -and $low -ge 1); Detail = "$n [campool] lines, large-pool free low-water $low of 8" }
    } }
    @{ Kind = 'Script'; Name = 'report: what the pre-fix code would have held (informational)'; Script = {
        param($ctx)
        $oldLow = $null; $smallLow = $null; $gyro = 0; $maxLive = 0; $first0 = $null; $i = 0
        foreach ($line in $ctx.LogLines) {
            $i++
            if ($line -match '\[campool\] \+ .* size 1632 ') { $gyro++ }
            if ($line -match 'small free (-?\d+) \(low (-?\d+)\) live (\d+) \| old large free (-?\d+) \(low (-?\d+)\)') {
                if ($null -eq $smallLow -or [int]$Matches[2] -lt $smallLow) { $smallLow = [int]$Matches[2] }
                if ([int]$Matches[3] -gt $maxLive) { $maxLive = [int]$Matches[3] }
                $v = [int]$Matches[5]; if ($null -eq $oldLow -or $v -lt $oldLow) { $oldLow = $v }
                if ($null -eq $first0 -and [int]$Matches[4] -le 0) { $first0 = $i }
            }
        }
        $td = @($ctx.LogLines | Where-Object { $_ -match '\[crashcam\] container current state -> 3\b' }).Count
        @{ Pass = $true; Detail = ("take-down states $td; GyroCams brought up $gyro; max live behaviours $maxLive; small-pool low $smallLow of 20; " +
                                   "pre-fix large free would have reached $oldLow" +
                                   $(if ($null -ne $first0) { " (0 at log line $first0 -- the old code asserted there on the next large allocation)" } else { "" })) }
    } }
)
$case
