# FX-BRIDGES CC-8 live case: a RIVAL takes the PLAYER down in real play, and the game-state -> director bridge
# must hand that to the director. BrnGameModule::BridgeGameStateToDirector @0x823CD170 walks the game state's
# takedown queue and, for an event whose victim is the player, raises the director input's mbPlayerTakenDown and
# records the killer (the inlined InputBuffer::SetPlayerKiller); MainDirector::ProcessInputQueue latches it and
# ArbStateCrashing allocates the authored taken-down camera on the killer. Before CC-8 the bridge only Appended
# the action queue, so the director never saw a taken-down frame.
#
# The drive is RivalOrganic's organic Road Rage pursuit (no injected takedown: its "[td] HARNESS" Max 0 check is
# kept) with two existing knobs:
#   BRN_AI_MADNESS=1  the PC harness override in AICar::SetRoadRageMadness (rival aggression 0.6), so the rivals
#                     attack the player -- the attacks, the player's crash and the takedown CREDIT stay organic;
#   BRN_TD_DIAG=1     the [td-detect] witnesses (which crash the takedown manager credited to whom).
# The witnesses (BRN_CRASHCAM_DIAG, armed by RivalOrganic):
#   [takedown-cam] BridgeGameStateToDirector: player slot P taken down by slot K ...   the producer (BrnGameModule.cpp)
#   [takedown-cam] director read: mbPlayerTakenDown 1 killer slot K                     the consumer's read
#                                                  (InputBuffer::GetPlayerKillerCarIndex, called only from
#                                                  ProcessInputQueue's taken-down arm)
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxBridgesTakedownCamLive.ps1 --run-name fxbridges_cc8
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxbridges_takedown_cam'
$case.Bug = 'A rival takedown of the player must reach the director input (mbPlayerTakenDown + killer), so the taken-down camera can play.'
$case.Run.MaxSeconds = 300
$case.DiagEnv += ',BRN_AI_MADNESS=1,BRN_TD_DIAG=1'
$keep = @('no new assertions', 'no exceptions', 'reached driving', 'no injected takedown')
$case.Checks = @($case.Checks | Where-Object { $keep -contains $_.Name })
$case.Checks += @(
    @{ Kind = 'Script'; Name = 'CC-8: a rival-on-player takedown reaches the director input (bridge publishes mbPlayerTakenDown + killer)'; Script = {
        param($ctx)
        $pairs = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[takedown-cam\] BridgeGameStateToDirector: player slot (\d+) taken down by slot (-?\d+) \(takedown type (-?\d+)\)') {
                "player $($Matches[1]) <- killer $($Matches[2]) type $($Matches[3])"
            }
        })
        $credited = @($ctx.LogLines | Where-Object { $_ -match '\[td-detect\] standard victim=\d+ aggressor=\d+' })
        @{ Pass = $pairs.Count -gt 0
           Detail = "$($pairs.Count) bridge line(s): $((@($pairs | Select-Object -Unique)) -join '; ') | $($credited.Count) [td-detect] standard credit line(s) (all victims)" }
    } }
    @{ Kind = 'Script'; Name = 'CC-8: the director consumes it (ProcessInputQueue reads mbPlayerTakenDown 1 and the same killer)'; Script = {
        param($ctx)
        $killers = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[takedown-cam\] BridgeGameStateToDirector: player slot \d+ taken down by slot (-?\d+)') { $Matches[1] }
        } | Select-Object -Unique)
        $reads = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[takedown-cam\] director read: mbPlayerTakenDown (\d) killer slot (-?\d+)') { "$($Matches[1])/$($Matches[2])" }
        })
        $good = @($reads | Where-Object { $_ -match '^1/(-?\d+)$' -and ($killers -contains ($_ -replace '^1/', '')) })
        @{ Pass = ($good.Count -gt 0 -and $good.Count -eq $reads.Count)
           Detail = "$($reads.Count) director read(s) [flag/killer]: $((@($reads | Select-Object -Unique)) -join ', ') vs bridge killer(s) $($killers -join ', ')" }
    } }
    @{ Kind = 'Script'; Name = 'CC-8: every player-taken-down HUD message (TDBD) had its director publish, and the crash window it landed in'; Script = {
        param($ctx)
        $lines = $ctx.LogLines
        $tdbd = 0; $bridge = 0; $inCrash = 0; $crashOpen = $false; $firstBridgeBeforeTdbd = $true
        for ($i = 0; $i -lt $lines.Count; $i++) {
            $l = $lines[$i]
            if ($l -match '\[crashcam\] mbCrashActive -> (\d)') { $crashOpen = ($Matches[1] -eq '1') }
            elseif ($l -match '\[takedown-cam\] BridgeGameStateToDirector:') { $bridge++; if ($crashOpen) { $inCrash++ } }
            elseif ($l -match 'STARTING MESSAGE NAMED "TDBD') { $tdbd++; if ($bridge -eq 0) { $firstBridgeBeforeTdbd = $false } }
        }
        @{ Pass = ($tdbd -eq 0 -or ($bridge -gt 0 -and $firstBridgeBeforeTdbd))
           Detail = "$tdbd TDBD message(s); $bridge bridge line(s), $inCrash of them while the player's crash window was open (mbCrashActive 1 -> ArbStateCrashing allocates the taken-down cam)" }
    } }
)
$case
