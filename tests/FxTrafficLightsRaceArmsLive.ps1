# FX-TRAFFICLIGHTS (crash parity wave 5, 2026-09-25) -- live witness of the traffic's two EVENT arms of
# TrafficEntityModule::HandleExternalRequests (b5 9c6d1844), which free roam never reaches:
#   30  E_ACTION_STOP_MODE_INTRO           (0x8274BC84) the event's intro ends: an event that clears the traffic
#                                           (mbGameModeClearsTraffic) sweeps KillAllTrafficInCylinder(car, 30.0f
#                                           flt_820BA5E8, 10.0f flt_820BA5E4, true) around every active race car,
#                                           and offline the start-line protection (+0x717E1) ends.
#   244 E_ACTION_HUD_MESSAGE_DIST_TO_FINISH (0x8274BF74) HUDMessageLogic posts one record per 500 m mark
#                                           (0x82395B84); a mark below 1505.0f (flt_820BA814; `bge` skips) halves
#                                           mfGameModeDensityScale (fmuls 0.5f, flt_820BA62C).
# Same race as FxAinan2CheckpointLive.ps1 / FxDirectorCheckpointLive.ps1 (junction 480886, -Teleport
# "3003.9,6.6,-1675.6,0" -StartEvent), driven by the game's own AI on the pad (--ai-pad race: the player finished
# 2nd at 49 s of mode time in fxaipad_race/20260925_102436).
# BRN_TRAFFIC_TRACK arms the [traffic-track] lines (FLAG PC, reads only).
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxTrafficLightsRaceArmsLive.ps1 --ai-pad race --no-frames --run-name fxtrafficlights_race_arms
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxtrafficlights_race_arms'
$case.Area = 'traffic'
$case.Bug = 'HandleExternalRequests arms 30 (the intro start-grid sweep) and 244 (the density halving inside 1505 m of the finish) were one gate before 9c6d1844.'
$case.Run.Teleport = '3003.9,6.6,-1675.6,0'
$case.Run.MaxSeconds = 150
$case.DiagEnv += ',BRN_TRAFFIC_TRACK=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = '30: the intro ended (start-grid sweep r 30 / h 10, start-line protection off)'
       Pattern = '\[traffic-track\] request 30 stop-mode-intro clears=[01] swept race cars=\d+ r=30(\.0+)? h=10(\.0+)? protect=0'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = '244: the 1500 m mark halves the density'
       Pattern = '\[traffic-track\] request 244 dist-to-finish d=1500(\.0+)? halved=1'; Expect = $true }
    @{ Kind = 'Script'; Name = '244: every mark below 1505 m halves the density, every other mark leaves it'; Script = {
        param($ctx)
        $marks = @(); $bad = @()
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[traffic-track\] request 244 dist-to-finish d=(?<d>[-\d.]+) halved=(?<h>[01]) density=(?<s>[-\d.eE+]+)') {
                $d = [double]$Matches.d; $h = $Matches.h
                $marks += ("{0}:{1}:{2}" -f $Matches.d, $h, $Matches.s)
                if (($d -lt 1505.0) -ne ($h -eq '1')) { $bad += $Matches.d }
            }
        }
        @{ Pass = ($marks.Count -gt 0 -and $bad.Count -eq 0)
           Detail = ("{0} mark(s) d:halved:density = {1}; wrong polarity: {2}" -f $marks.Count, ($marks -join ' '), ($bad -join ' ')) }
    } }
    @{ Kind = 'LogCount'; Name = 'no assert from the traffic module'; Pattern = '\[ASSERT \d+\].*BrnTrafficEntityModule'; Max = 0 }
)
$case
