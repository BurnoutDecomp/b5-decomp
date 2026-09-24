# FX-TAILS-A item 2 live case (crash parity 2026-09-24): GameStateModule::ClearData @0x8236B3A8 runs at its two
# console seats -- Construct (0x823807A8) and Prepare's START stage (0x8239E6A8) -- and the boot, junkyard, car
# select and a drive still behave, with no new assert family.
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxTailsAClearDataLive.ps1
# Witness that the new code was dispatched: Prepare's own stage-0 line (printed under gxMessageFilterFlags & 1),
#   `[GameStateModule::Prepare] stage 0 -- ClearData + Reset Player Car registered; GameState debug deferred`
# Before (b5 36b0c061 and earlier) the line read "Reset Player Car registered; ClearData + GameState debug deferred"
# and ClearData had no body: its values rode as member initialisers and two early interface Clears.
# What ClearData changes at runtime, all console values: the player's active / global race-car indices read -1
# (not the static zero-fill 0) until the first post-world snapshot, the cached AI car interface reads FLT_MAX /
# 0x7FFF, and Prepare's START stage clears the pause flags, the streaming / junkyard handshake, the carry queue,
# the showtime hand-off and the junction cache.
@{
  Name   = 'fxtailsa_clear_data'
  Area   = 'gamestate'
  Bug    = 'GameStateModule::ClearData had no body, so neither Construct nor Prepare''s START stage reset the module''s per-session state the way the console does.'
  Frames = $false
  Run    = @{
    Drive       = $true
    MotionProbe = $true
    MaxSeconds  = 60
    SkipIntro   = $true
    AcceptGap   = 1.0
    Teleport    = '3040.7,-5.8,-1937.9,180'   # the road outside the junkyard exit (baseline_boot_drive)
  }
  DiagEnv = ''
  Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'LogMatch';   Name = 'Prepare''s START stage ran ClearData (the new stage-0 witness)';
       Pattern = '\[GameStateModule::Prepare\] stage 0 -- ClearData \+ Reset Player Car registered'; Expect = $true }
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'Script';     Name = 'the car moved'; Script = {
        param($ctx)
        $l = ($ctx.MarksText -split "`n") | Where-Object { $_ -match '^DRIVE\s+' } | Select-Object -First 1
        if (-not $l) { return @{ Pass = $false; Detail = 'no DRIVE line in marks.txt' } }
        $ok = ($l -match 'path=(?<p>[\d.,]+)m') -and ([double](($Matches.p) -replace ',', '.') -gt 20)
        return @{ Pass = $ok; Detail = $l.Trim() }
    } }
  )
}
