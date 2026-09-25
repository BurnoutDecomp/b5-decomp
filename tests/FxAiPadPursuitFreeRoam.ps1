# FX-AIPAD (crash parity 2026-09-25) -- the "AI PAD" seat, PURSUIT objective, FREE ROAM, live. SLOT 3 ONLY.
#
# BRN_AI_PAD_PLAYER=pursuit: the game's OWN AI drives the player car through the pad path (control word stays 1,
# see FxAiPadCruiseLive.ps1) and AIModule::HarnessAIPadPursuit (BrnAIModule_Drive.cpp, harness code) aims it:
#   TARGET  the nearest ATTACHED rival -- a car of the roster AIModule::UpdateCars walks (35 in free roam,
#           0x8279A580) that is attached to AI control (IN_RANGE with a driver or OUT_OF_RANGE without one), kept
#           until it crashes, is reset on track or leaves;
#   ROUTE   the console's own A* request for the player: RACE route-finding style + the target's section
#           (RouteRequestManager::GenerateRoute @0x827948B0 -> GenerateStandardRouteRequest @0x82791490);
#   RAM     inside the console's slam window (20 m, -4.5..20 m ahead: IncludeSmashIntoPlayer @0x82791230,
#           flt_820C8074 / flt_820C4890 / flt_820047C8) the player's AI gets the Slam fan bias with the target as
#           its victim, and the pad holds throttle + boost.
# Free-roam rivals exist only on a PROGRESSION profile: slot 0's harness save is fresh (profileRivals=0).
# build/game/Memcard_3 holds a copy of the progression profile (6 rivals, 5 unlocked), so run it on slot 3
# while holding the slot-0 box lock (the lane recipe):
#   . tools\diagnostics\_box_lock.ps1; Enter-BoxLock -TimeoutSec 7200 -Label FX-AIPAD-slot3 -Slot 0; & tools\tests\run_case.ps1 -Case b5-decomp/tests/FxAiPadPursuitFreeRoam.ps1 -Slot 3
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxAiPadPursuitFreeRoam.ps1 --ai-pad pursuit --slot 3 --run-name fxaipad_freeroam
# First run: fxaipad_freeroam/20260925_105030 GREEN 12/12 on slot 3 -- 5 rivals posted; target global 3 at 2443 m
# (OUT_OF_RANGE), 123 route plans over 5740 m, no stationary spell, no unstick; rivals 2 and 5 streamed in near the
# player (ACTIVATE_RACE_CAR) and the RAM window opened 5 times, no contact; the rivals read pDrv 1 on all 257
# in-range samples and FindTarget took the player ([mm-ai] tgt 1) on 54 of 224.
# Witnesses (NOT X360): `[rivals] update: ... posted=N` (BRN_PROGRESSION_RIVALS), `[ai-pad] pursuit: target -> global G
# (..., IN_RANGE|OUT_OF_RANGE; n attached rival(s))`, `[ai-pad] pursuit: route #n section A -> B`, `RAM window entered`,
# the rivals' `[rival] ... pDrv 1` (the player is still a pad-driven target to them) and `[mm-ai] ... tgt 1`.
@{
    Name  = 'fxaipad_freeroam'
    Area  = 'harness'
    Bug   = 'The AI PAD seat pursues a free-roam rival with the game''s own route planner and slam fan (control word stays 1).'
    Frames = $false
    Run = @{
        Drive       = $true
        MotionProbe = $true
        SkipIntro   = $true
        AcceptGap   = 1.0
        Teleport    = '3040.7,-5.8,-1937.9,180'
        MaxSeconds  = 240
    }
    DiagEnv = 'BRN_AI_PAD_PLAYER=pursuit,BRN_PROGRESSION_RIVALS=1,BRN_RIVAL_PURSUIT_DIAG=1,BRN_RIVAL_DAMAGE_DIAG=1,BRN_CRASHCAM_DIAG=1,BRN_MM_DIAG=1'
    Checks = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached DRIVING'; Phase = 'DRIVING' }
        @{ Kind = 'LogValue'; Name = 'the slot''s profile put rivals in the world (run it on slot 3)'
           Pattern = '\[rivals\] update: profileRivals=\d+ unlocked=\d+ posted=(?<n>\d+)'; Group = 'n'; Agg = 'max'; Min = 1 }
        @{ Kind = 'LogMatch'; Name = 'the AI PAD seat armed (pursuit)'; Pattern = '\[ai-pad\] \*\*\*\*\* HARNESS-ONLY \(BRN_AI_PAD_PLAYER=pursuit\): armed'; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'the pursuit took an attached rival as its target'; Pattern = '\[ai-pad\] pursuit: target -> global \d+'; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'the pursuit routed the player with the console''s planner'; Pattern = '\[ai-pad\] pursuit: route #\d+ section \d+ -> \d+'; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'the pursuit closed in (the console''s slam window)'; Pattern = '\[ai-pad\] pursuit: RAM window entered'; Expect = $true }
        @{ Kind = 'Script'; Name = 'the rivals still read the player as pad-driven (pDrv 1 on every in-range sample)'; Script = {
            param($ctx)
            $one = 0; $zero = 0
            foreach ($line in $ctx.LogLines) {
                if ($line -match '^\[rival\] .* state 0 .* pDrv (\d)') { if ($Matches[1] -eq '1') { $one++ } else { $zero++ } }
            }
            @{ Pass = ($one -gt 0 -and $zero -eq 0); Detail = "in-range rival samples: pDrv 1 x$one, pDrv 0 x$zero" }
        } }
        @{ Kind = 'Script'; Name = 'report: contacts and credited takedowns (informational)'; Script = {
            param($ctx)
            $td = @($ctx.LogLines | Where-Object { $_ -match '\[ai-evt\] action 14 ON_PLAYER_TAKEDOWN' }).Count
            $victims = @($ctx.LogLines | ForEach-Object { if ($_ -match '\[rival-damage\] player-takedown victim=(\d+)') { $Matches[1] } }) -join ','
            $contacts = @($ctx.LogLines | Where-Object { $_ -match '\[td-contact\] entry race car 0 vs |\[td-contact\] entry race car \d+ vs 0 ' }).Count
            $rams = @($ctx.LogLines | Where-Object { $_ -match '\[ai-pad\] pursuit: RAM window entered' }).Count
            @{ Pass = $true; Detail = "RAM window entries $rams; player race-car contact lines $contacts (capped at 16); player-credited takedowns $td (victims $victims)" }
        } }
    )
}
