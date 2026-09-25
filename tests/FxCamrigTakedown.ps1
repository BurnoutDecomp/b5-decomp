# FX-CAMRIG takedown witness (crash-parity CC-4): the organic rival-takedown drive of RivalDamage.ps1 with the rig's
# BRN_CAMRIG_DIAG line switched on.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxCamrigTakedown.ps1 --ai-pad pursuit --no-frames --run-name fxcamrig_takedown
# The seat is the game's own AI on the pad (--ai-pad pursuit, FX-AIPAD 2026-09-25): fxcamrig_takedown_aipad/20260925_105739
# PASS 15/15 with 3 player-credited takedowns; the bearing-steering pad pursuit's run 20260924_214200 had none
# (LIVE_FINAL: DRIVER-LIMITED). Without --ai-pad the runner still drives the old way.
# Why a takedown run for a Showtime camera: ArbStateTakedown::Prepare allocates a SECOND instance of
# BehaviourAftertouchCrash -- mTakedownDebugCam, on the bank's mCrashDebugParams block -- and flags it DisableCollision +
# SetIsTempDebugCrashCamera (+0x3C2 / +0x3C3). The behaviour manager updates every allocated behaviour each frame, so
# since the rig got its body that instance runs Update's temp-debug arms (the first-frame RegisterImpact kick and the
# debug-param framing walk) on every takedown. This case proves it is dispatched there (`tempDebug 1`), stays finite,
# and fires no assertion. Frames are off: the takedown state publishes its own cameras, not the debug-cam's.
$case = & (Join-Path $PSScriptRoot 'RivalDamage.ps1')
$case.Name = 'fxcamrig_takedown'
$case.Frames = $false
$case.DiagEnv += ',BRN_CAMRIG_DIAG=1'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'the takedown debug-cam instance ran the rig (first frame, temp-debug arm)';
       Pattern = '\[camrig\] Update frame 0 first 1 tempDebug 1 '; Expect = $true }
    @{ Kind = 'Script'; Name = 'every temp-debug rig frame is finite'; Script = {
        param($ctx)
        $inv = [Globalization.CultureInfo]::InvariantCulture
        $rx = '\[camrig\] Update frame \d+ first \d tempDebug 1 manual \d car (\S+) (\S+) (\S+) cam (\S+) (\S+) (\S+) dir (\S+) (\S+) (\S+) dist (\S+) height (\S+)'
        $n = 0; $bad = 0
        foreach ($l in $ctx.LogLines) {
            if ($l -match $rx) {
                $n++
                for ($i = 1; $i -le 11; $i++) {
                    $v = 0.0
                    if (-not [double]::TryParse($Matches[$i], [Globalization.NumberStyles]::Float, $inv, [ref]$v) -or
                        [double]::IsNaN($v) -or [double]::IsInfinity($v)) { $bad++; break }
                }
            }
        }
        @{ Pass = ($n -gt 0 -and $bad -eq 0); Detail = "$n temp-debug [camrig] lines, $bad with a non-finite field" }
    } }
)
$case
