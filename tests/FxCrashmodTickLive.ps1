# FX-CRASHMOD (crash parity 2026-09-23), G63-D1 live witness. RaceCarCrash::Tick @0x827BF0B8 tests
# the player's own wreck for "still moving" with |mfSpeedMPH| > 6.5 or |mAngularVelocity| > 1.5
# (RaceCarState+0x340, 0x827BF20C) at the 0.25 s-before-cleanup edge. The [crash-tick] line
# (BRN_CRASH_ACTION_DIAG, armed by CrashActions.ps1) proves that edge was dispatched in this run and
# prints the spin that fed it. The player must crash organically during the rival pursuit.
# Built on CrashActions.ps1, NOT CrashEnding.ps1: whether event 42 survives the frame depends on
# which crash record is ticked LAST (TickCrashes 0x827C66E4 hands &mbNeedToSendEndingMessage to
# every Tick and Tick stores it unconditionally at 0x827BF304 -- a console quirk), so the
# crash-ending checks fail organically whenever another wreck sits after the player's record.
$case = & (Join-Path $PSScriptRoot 'CrashActions.ps1')
$case.Name = 'fxcrashmod_tick'
$case.Bug = 'The player wreck''s cleanup extension must test its angular velocity (RaceCarState+0x340), not its linear velocity.'
$case.Checks += @(
    @{ Kind = 'LogCount'; Name = 'player wreck reached the 0.25 s decision edge'
       Pattern = '\[crash-tick\] player wreck edge owner='; Min = 1 }
)
$case
