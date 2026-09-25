# FX-DIRECTOR2 (crash parity 2026-09-25) -- the jump moment (type 7) is allocated and ticked in free roam, and stays
# shut, as on the console.
#
# ArbStateRoaming::Construct @0x82259C00 registers {7, 0}, {8, 0}, {10, 0}. The PC's NewMoment returned TRUE for type 7
# with the handle unallocated (a GATE), so Roaming held two moments where the console holds three. Now the moment is
# allocated and ticked. On retail its SEARCHING arm never passes: the third gate, MainDirector::mbAllowJumpMoment, is
# seeded false (0x8225B97C) and never written. The camera side behind that gate is a LOUD trap ([pjump] TRAP), which a
# retail drive must never print.
# The drive: the AI PAD cruise seat (the game's own AI steers the player car) from the FxAiPadCruiseLive seat.
# Witnesses (NOT X360, BRN_CRASHCAM_DIAG):
#   [jump-ladder] MomentController::NewMoment allocated type=7 paramID=0 allocated=1   -- allocated
#   [pjump] SEARCHING tick 1 / tick 300: ... allowJumpMoment=0                         -- ticked, and shut
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxDirector2JumpMomentLive.ps1
@{
    Name  = 'fxdirector2_jump_moment'
    Area  = 'director'
    Bug   = 'The jump moment (type 7) must be allocated and ticked in free roam like the console''s, and stay shut (mbAllowJumpMoment is false on retail).'
    Frames = $false
    Run = @{
        Drive       = $true
        MotionProbe = $true
        SkipIntro   = $true
        AcceptGap   = 1.0
        Teleport    = '3040.7,-5.8,-1937.9,180'
        MaxSeconds  = 110
    }
    DiagEnv = 'BRN_AI_PAD_PLAYER=cruise,BRN_CRASHCAM_DIAG=1'
    Checks = @(
        @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
        @{ Kind = 'LogCount';   Name = 'NewMoment allocated the jump moment (type 7, Roaming''s {7, 0})'
           Pattern = '\[jump-ladder\] MomentController::NewMoment allocated type=7 paramID=0 allocated=1'; Min = 1 }
        @{ Kind = 'LogCount';   Name = 'Roaming''s other two moments are allocated too (types 8 and 10)'
           Pattern = '\[jump-ladder\] MomentController::NewMoment allocated type=(8|10) paramID=0 allocated=1'; Min = 2 }
        @{ Kind = 'LogCount';   Name = 'the jump moment ticked (its first SEARCHING tick)'; Pattern = '^\[pjump\] SEARCHING tick 1: '; Min = 1 }
        @{ Kind = 'LogCount';   Name = 'the jump moment kept ticking (its 300th SEARCHING tick)'; Pattern = '^\[pjump\] SEARCHING tick 300: '; Min = 1 }
        @{ Kind = 'LogCount';   Name = 'every tick witness shows the retail gate shut (allowJumpMoment=0)'
           Pattern = '^\[pjump\] SEARCHING tick \d+: .* allowJumpMoment=1'; Max = 0 }
        @{ Kind = 'LogCount';   Name = 'no jump-moment trap fired (retail never reaches the camera side)'; Pattern = '\[pjump\] TRAP'; Max = 0 }
        @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
        @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
        @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    )
}
