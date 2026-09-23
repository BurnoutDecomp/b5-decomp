# FX-CRASHMOD (crash parity 2026-09-23), G63-D1 live witness. RaceCarCrash::Tick @0x827BF0B8 tests
# the player's own wreck for "still moving" with |mfSpeedMPH| > 6.5 or |mAngularVelocity| > 1.5
# (RaceCarState+0x340, 0x827BF20C) at the 0.25 s-before-cleanup edge. The [crash-tick] line
# (BRN_CRASH_ACTION_DIAG, armed by CrashActions.ps1) proves that edge was dispatched in this run and
# prints the spin that fed it. Built on CrashEnding.ps1: the player must crash organically.
$case = & (Join-Path $PSScriptRoot 'CrashEnding.ps1')
$case.Name = 'fxcrashmod_tick'
$case.Bug = 'The player wreck''s cleanup extension must test its angular velocity (RaceCarState+0x340), not its linear velocity.'
$case.Checks += @(
    @{ Kind = 'LogCount'; Name = 'player wreck reached the 0.25 s decision edge'
       Pattern = '\[crash-tick\] player wreck edge owner='; Min = 1 }
)
$case
