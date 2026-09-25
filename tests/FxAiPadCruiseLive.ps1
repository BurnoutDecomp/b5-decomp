# FX-AIPAD (crash parity 2026-09-25) -- the "AI PAD" seat, cruise objective, free roam, live.
#
# BRN_AI_PAD_PLAYER=cruise: the game's OWN AI computes the player car's controls (the console seat's
# driving -- WorldDebugComponent::AIDrivesPlayerChanged @0x827B1FC0's AI, with the player's AICar reading
# mbIsDrivenByPlayer = 0 for its own update only) and they reach physics through the PAD path:
# WorldModule::HarnessApplyAIPad writes them into the race-car pre-scene PlayerVehicleControls, the
# console's ProcessPlayerVehicleInput @0x822FFE30 builds the PLAYER record, and the control word stays 1
# (E_CAR_CONTROL_ENTITY_MODULE), so the console's `== 1` pre-physics bridge forwards it
# (0x827AAFE8..0x827AB008) and the AI bridge drops the AI record (== 2 gate, 0x827AAC08..0x827AAC2C).
# -Drive only trips the teleport arm: once the seat is armed the AI's throttle replaces the held one.
# Witnesses (NOT X360): `[ai-pad] ***** HARNESS-ONLY ... armed`, one `[ai-pad] pad #n ...` line per second of
# applied pad, `[player-ai] control=1 ...` (BRN_PLAYER_AI_DIAG: the player's AI record, read AFTER the seat's own
# update -- the byte every other reader sees is back to 1 while the record steers), and the [motion] path in
# marks.txt.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxAiPadCruiseLive.ps1
@{
    Name  = 'fxaipad_cruise'
    Area  = 'harness'
    Bug   = 'The AI PAD seat: the game''s own AI drives the player car through the pad path (control word stays 1).'
    Frames = $false
    Run = @{
        Drive       = $true
        MotionProbe = $true
        SkipIntro   = $true
        AcceptGap   = 1.0
        Teleport    = '3040.7,-5.8,-1937.9,180'
        MaxSeconds  = 130
    }
    DiagEnv = 'BRN_AI_PAD_PLAYER=cruise,BRN_PLAYER_AI_DIAG=1'
    Checks = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached DRIVING'; Phase = 'DRIVING' }
        @{ Kind = 'LogMatch'; Name = 'the AI PAD seat armed'; Pattern = '\[ai-pad\] \*\*\*\*\* HARNESS-ONLY \(BRN_AI_PAD_PLAYER=cruise\): armed'; Expect = $true }
        @{ Kind = 'LogCount'; Name = 'the console seat was never forced (no BRN_AI_DRIVES_PLAYER arm)'; Pattern = '\[ai-drive\]'; Max = 0 }
        @{ Kind = 'LogCount'; Name = 'the pad carried the AI for a minute or more'; Pattern = '\[ai-pad\] pad #\d+ cruise'; Min = 60 }
        @{ Kind = 'Script'; Name = 'the AI steered (non-zero steering on the pad)'; Script = {
            param($ctx)
            $n = 0; $total = 0
            foreach ($line in $ctx.LogLines) {
                if ($line -match '\[ai-pad\] pad #\d+ cruise gas ([-0-9.e]+) brake ([-0-9.e]+) hb ([-0-9.e]+) steer ([-0-9.e]+)') {
                    $total++
                    if ([math]::Abs([double]::Parse($Matches[4], [cultureinfo]::InvariantCulture)) -gt 0.01) { $n++ }
                }
            }
            @{ Pass = ($n -gt 0); Detail = "$n of $total pad samples steer by more than 0.01" }
        } }
        @{ Kind = 'Script'; Name = 'the car drove (marks DRIVE path > 1000 m)'; Script = {
            param($ctx)
            $l = ($ctx.MarksText -split "`n") | Where-Object { $_ -match '^DRIVE\s+' } | Select-Object -First 1
            if (-not $l) { return @{ Pass = $false; Detail = 'no DRIVE line in marks.txt' } }
            $ok = ($l -match 'path=(?<p>[\d.,]+)m') -and ([double](($Matches.p) -replace ',', '.') -gt 1000)
            @{ Pass = $ok; Detail = $l.Trim() }
        } }
    )
}
