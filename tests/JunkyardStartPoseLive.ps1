# L4 boot order (conductor 2026-09-28, L2's crash-sweep cell h225_s80) -- a FRESH profile starts at the track's start
# junkyard, and the sweep cell fires its shot from there again.
#
# THE CONSOLE. GameStateModule::SendSetupPlayerCarEvent @0x8239A918 (the start-of-game one-shot, the first
# loading-scripted frame) stores the TriggerData player-start pose into the PROFILE's car pose: `stvx128 v127, r31,
# 0xBCD0` @0x8239A97C / `stvx128 v0, r31, 0xBCE0` @0x8239A980 == Profile(this+0xBCA0)+0x30 / +0x40. At the MemoryCard
# exit OnProfileLoaded @0x82397310 enters the junkyard nearest that pose (FindNearestJunkyardID, `lvx128 v1, r30,
# 0x30` @0x823973D0). A fresh profile has no Deserialise, so it keeps the seeded start pose -> junkyard 250700.
# THE PC dropped the two stores; with f935feb8's OnProfileLoaded a fresh profile kept Profile::Construct's (0,0,0)
# and entered 312262, the junkyard nearest the ORIGIN: l4_cell_h225_s80_fresh/20260928_090320 and _090518 (exe
# 0dd8736dddc2) log `OnProfileLoaded junkyard=312262 ... savedPos=(0.000000, 0.000000, 0.000000)` and the sweep's shot
# `from (-299.567108, 8.158300, 923.594910)`, 4.8 km from the cell's wall.
#
# The run: the cell exactly as tools/diagnostics/crash_sweep_batch.ps1 builds it (-MinDamageableSeconds 1.6: launch
# 3261.11,-3.7,-1914.09, shot 225:80, arm 4 m), on a FRESH profile (this slot's save is parked and put back).
# NO FRAME DUMP.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/JunkyardStartPoseLive.ps1 -Slot <n>

$KV_START = @(2960.891113, 1.866618, -1658.474976)     # the logged TriggerData player start ("playerStart=")
$KV_JUNKYARD_250700 = @(2986.933105, -2011.417969)       # junkyard 250700's logged spawn[1] (x, z)

@{
  Name    = 'junkyard_start_pose'
  Area    = 'flow'
  Bug     = 'A fresh profile must start at the track''s start junkyard (250700): the one-shot seeds the profile''s car pose, which OnProfileLoaded reads.'
  Frames  = $false
  FreshProfile = $true
  Run     = @{
    Drive           = $true
    CrashSweep      = '3261.11,-3.7,-1914.09'
    CrashSweepShots = '225:80'
    CrashSweepArm   = 4
    MaxSeconds      = 110
  }
  DiagEnv = 'BRN_CRASH_RESPONSE_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the boot is a FRESH profile'; Pattern = '^\[profile-save\] deserialised: isNewProfile=1 '; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the one-shot enters the start junkyard 250700 from the TriggerData start position'
       Pattern = '^\[GameStateModule::SendSetupPlayerCarEvent\] junkyard=250700 .* playerStart=\(2960\.891113, 1\.866618, -1658\.474976\)'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'OnProfileLoaded reads the SEEDED pose and enters 250700 (not the origin''s 312262 with savedPos=(0,0,0))'
       Pattern = '^\[GameStateModule::OnProfileLoaded\] junkyard=250700 .* savedPos=\(2960\.891113, 1\.866618, -1658\.474976\)'; Min = 1; Max = 1 }
    @{ Kind = 'Script';   Name = 'the sweep cell''s shot fires from near junkyard 250700 again (the car was not teleported across the map)'; Script = {
        param($ctx)
        $line = $ctx.LogLines | Where-Object { $_ -match '^\[sweep\] shot 0/1 .* from \((-?[0-9.]+), (-?[0-9.]+), (-?[0-9.]+)\)' } | Select-Object -First 1
        if (-not $line) { return @{ Pass = $false; Detail = 'no [sweep] shot 0/1 line' } }
        $null = $line -match 'from \((-?[0-9.]+), (-?[0-9.]+), (-?[0-9.]+)\)'
        $inv = [Globalization.CultureInfo]::InvariantCulture
        $x = [double]::Parse($Matches[1], $inv); $z = [double]::Parse($Matches[3], $inv)
        $d = [Math]::Sqrt([Math]::Pow($x - $KV_JUNKYARD_250700[0], 2) + [Math]::Pow($z - $KV_JUNKYARD_250700[1], 2))
        return @{ Pass = ($d -lt 500.0); Detail = ("shot fired from ({0}, {1}): {2:F0} m from junkyard 250700" -f $Matches[1], $Matches[3], $d) }
      }.GetNewClosure() }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
