# L3 RACEINTRO (2026-09-28) live case: the start light's lamps go RED, AMBER, GREEN through a race countdown.
#
# The owner's report: "The races intro doesn't show the traffic light going green". A traffic light's lit lamps are its
# coronas. On the PC the lights cycled (0802404c) and followed the event countdown (30d481f4), but no corona was ever
# drawn for them: TrafficEntityModule::RenderTrafficLightCoronas @0x8271EC80 (GenerateDispatchLists, bl 0x8273B4C4) and
# the manager's RenderLightsForHull @0x8275DBF0 / RenderAllLightsToBeInStateForHull @0x8275DE50 had no body.
# The countdown chain: ModeManager::CheckCountdownDisplay posts E_ACTION_SET_COUNTDOWN -> HandleExternalRequests arm 47
# -> TrafficLightManager::SetCountdownValue (RED at 3 and 2, AMBER at 1, GREEN at 0 for 3 s) -> RenderLightsForHull draws
# every light of every active hull in the countdown's state.
# The recipe is FxRaceIntroTrafficLightSpaceLive's offline race at junction 480886. The countdown take's TRAFFIC_LIGHT
# interval frames the start light from below (Part A, d533f7e7). Its instance is 117, the light CalcTrafficLightSpace
# chose there ([tl-space] ... chose 117).
# Witnesses (all NOT X360, all default-off):
#   [tl-corona] instance I <renderer> [countdown] mask M      BRN_TRAFFIC_LIGHT_CORONA_DIAG=<I>, on each change (L3)
#   [tl-coronas] pass N: active hulls A | frustum cells ... | other hulls GREEN G    the same variable, every 120th pass
#   [traffic-lights] SetCountdownValue display=D -> ...       BRN_TRAFFIC_LIGHT_DIAG (FX-NETCRASH)
#   [corona] atlas bound: ... -> pass READY                   always on (the corona renderer can draw)
# Frames every 12th present: the lit start light in the TRAFFIC_LIGHT interval, and the junction's lights before it.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxTrafficLightCoronasLive.ps1 -Slot 4
@{
    Name = 'fxtrafficlight_coronas'
    Area = 'traffic'
    Bug = 'The traffic lights'' lamps (coronas) were never drawn: the start light never showed RED, AMBER or GREEN through a race countdown, and no free-roam light was lit.'
    Frames = $true
    Run = @{
        Drive = $true
        Boost = ''
        MotionProbe = $true
        SkipIntro = $true
        AcceptGap = 1.0
        Teleport = '3003.9,6.6,-1675.6,0'
        StartEvent = $true
        EventFsm = $true
        SkipTrainingTip = $true
        MaxSeconds = 50
        FrameEvery = 12
    }
    DiagEnv = 'BRN_TRAFFIC_LIGHT_DIAG=1,BRN_TRAFFIC_LIGHT_CORONA_DIAG=117,BRN_INTRO_TIMER_DIAG=1,BRN_CAMERA_TRACE=1,BRN_ICE_TRACE=1,BRN_CRASHCAM_DIAG=1'
    Checks = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'LogCount'; Name = 'no assert from the light manager, the collection or the traffic module'
           Pattern = '\[ASSERT \d+\].*(BrnTrafficLightManager|BrnTrafficLightCollection|BrnTrafficEntityModule|CgsCamera)'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
        @{ Kind = 'LogMatch'; Name = 'the corona renderer can draw (the atlas is bound)'; Pattern = '\[corona\] atlas bound: .* READY'; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'the traffic-light corona pass runs over the active hulls (RenderTrafficLightCoronas, off the shadow pass)'
           Pattern = '\[tl-coronas\] pass \d+: active hulls [1-9]'; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'the pass draws the frustum''s other hulls GREEN'
           Pattern = '\[tl-coronas\] pass \d+: .* other hulls GREEN [1-9]'; Expect = $true }
        @{ Kind = 'Script'; Name = 'the event countdown reached the lights: SetCountdownValue display 3, 2, 1, 0'; Script = {
            param($ctx)
            $seen = @()
            foreach ($l in $ctx.LogLines) { if ($l -match '\[traffic-lights\] SetCountdownValue display=(-?\d+)') { $seen += [int]$Matches[1] } }
            $ok = ($seen -contains 3) -and ($seen -contains 2) -and ($seen -contains 1) -and ($seen -contains 0)
            @{ Pass = $ok; Detail = "displays: $($seen -join ',')" }
        } }
        @{ Kind = 'Script'; Name = 'the start light (instance 117) is drawn RED, then AMBER, then GREEN through the countdown'; Script = {
            param($ctx)
            $masks = @()
            foreach ($l in $ctx.LogLines) {
                if ($l -match '^\[tl-corona\] instance 117 RenderLightsForHull countdown mask (\d+)') { $masks += [int]$Matches[1] }
            }
            $i1 = [array]::IndexOf($masks, 1); $i2 = [array]::IndexOf($masks, 2); $i4 = [array]::IndexOf($masks, 4)
            $ok = ($i1 -ge 0) -and ($i2 -gt $i1) -and ($i4 -gt $i2)
            @{ Pass = $ok; Detail = "countdown masks in order: $($masks -join ' -> ') (1 RED, 2 AMBER, 4 GREEN)" }
        } }
        @{ Kind = 'LogMatch'; Name = 'the countdown ends: the lights go back to their own phases (TrafficLightManager::Update @0x827517A8)'
           Pattern = '\[traffic-lights\] countdown over -> countdown=0'; Expect = $true }
        # After GO the start light is drawn in its own phase -- unless a car knocks it down first: a rival on the grid can
        # hit its pole within the 3 s GREEN (20260928_063908: rival slot 1 at 3 m, then "smashed: not drawn"). A smashed
        # light is not drawn, as on the console (the flags byte's 0x80, 0x8275DD30), so either outcome is the console's.
        @{ Kind = 'Script'; Name = 'after the countdown GREEN the start light shows its own phase, or is reported smashed (then not drawn)'; Script = {
            param($ctx)
            $afterGreen = $false; $own = @(); $smashed = $false
            foreach ($l in $ctx.LogLines) {
                if ($l -match '^\[tl-corona\] instance 117 RenderLightsForHull countdown mask 4') { $afterGreen = $true; continue }
                if (-not $afterGreen) { continue }
                if ($l -match '^\[tl-corona\] instance 117 \S+ (countdown )?smashed: not drawn') { $smashed = $true; continue }
                if ($l -match '^\[tl-corona\] instance 117 \S+ mask (\d+)') { $own += [int]$Matches[1] }
            }
            @{ Pass = ($own.Count -gt 0 -or $smashed); Detail = "own masks after the GREEN: $($own -join ','); smashed: $smashed" }
        } }
        @{ Kind = 'Script'; Name = 'the race reaches E_GMS_IN_PROGRESS after the countdown'; Script = {
            param($ctx)
            $c = @($ctx.LogLines | Where-Object { $_ -match '\[stunt\] mode state -> E_GMS_IN_PROGRESS' }).Count
            @{ Pass = ($c -gt 0); Detail = "$c E_GMS_IN_PROGRESS rung(s)" }
        } }
    )
}
