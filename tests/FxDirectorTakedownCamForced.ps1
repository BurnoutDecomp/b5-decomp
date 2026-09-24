# FX-DIRECTOR live case (crash parity 2026-09-24, conductor item C2): the CAMERA end of the taken-down chain.
# FxBridgesTakedownCamForcedLive.ps1 (FX-BRIDGES CC-8: the reversed force-takedown harness credits a rival with a
# takedown of the player; the bridge publishes mbPlayerTakenDown + the killer; ProcessInputQueue reads them) plus the
# witness that ArbStateCrashing::UpdateActive then STARTS the authored taken-down ICE camera on that killer
# (BrnArbStateCrashing.cpp: NewBehaviour<BehaviourIceAnim> + the "takendown" shot group's first shot + the killer as
# the secondary vehicle ref -- the console's UpdateActive arm on GameState::mbPlayerWasTakenDown, +0x1C3).
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxDirectorTakedownCamForced.ps1 --run-name fxdirector_takedown_cam
# Witness (BRN_CRASHCAM_DIAG, armed by RivalOrganic, NOT X360):
#   [takedown-cam] ArbStateCrashing: taken-down ICE camera started on killer slot K allocated 1 takendown shots N shot 0 meState S
$case = & (Join-Path $PSScriptRoot 'FxBridgesTakedownCamForcedLive.ps1')
$case.Name = 'fxdirector_takedown_cam_forced'
$case.Bug = 'A rival takedown of the player that reaches the director must start the taken-down ICE camera on the killer.'
$case.Checks += @(
    @{ Kind = 'Script'; Name = 'C2: ArbStateCrashing starts the taken-down ICE camera on the killer the bridge published'; Script = {
        param($ctx)
        $killers = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[takedown-cam\] BridgeGameStateToDirector: player slot \d+ taken down by slot (-?\d+)') { $Matches[1] }
        } | Select-Object -Unique)
        $starts = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[takedown-cam\] ArbStateCrashing: taken-down ICE camera started on killer slot (-?\d+) allocated (\d) takendown shots (-?\d+)') {
                "$($Matches[1])/$($Matches[2])/$($Matches[3])"
            }
        })
        $good = @($starts | Where-Object { $_ -match '^(-?\d+)/1/([1-9]\d*)$' -and ($killers -contains ($_ -split '/')[0]) })
        @{ Pass = ($good.Count -gt 0 -and $good.Count -eq $starts.Count)
           Detail = ("{0} camera start(s) [killer/allocated/shots]: {1} vs bridge killer(s) {2}" -f
                     $starts.Count, ((@($starts | Select-Object -Unique)) -join ', '), ($killers -join ', ')) }
    } }
)
$case
