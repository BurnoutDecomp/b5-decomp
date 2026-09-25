# Live integration for the Showtime contact response. The numeric regression is
# run_showtime_impulse.py; this case requires the mode and contact chain to execute.
# A fresh fixture profile is used. Only the road-rule unlock gate is bypassed;
# mode entry still uses the normal both-bumpers input and physics contacts.
@{
    Name = 'showtime_contacts'
    Area = 'physics'
    Bug = 'Showtime contact impulses must run through the live mode without assertions or exceptions.'
    FreshProfile = $true
    Frames = $false
    Run = @{
        Drive = $true
        MotionProbe = $true
        SkipIntro = $true
        SkipTrainingTip = $true
        AcceptGap = 1.0
        Teleport = '3323.9,-2.4,-1793.2,0'
        # Let the new-profile driving introduction release controls and reach 10 m/s.
        # FX-DIRECTOR2 2026-09-25: 49 (was 40) and MaxSeconds 149 (was 140). The shift makes room for the crash
        # analyser, which MainDirector::PreSceneQueryUpdate calls unconditionally (0x8225BCDC).
        # Why the gesture had to move:
        #   - This drive hits a wall at about DRIVING+33 s. It is the first crash after boot, so it is forced HIGH
        #     (mbForceNextWorldCrashToBeFastTopDown, a one-shot cleared at 0x822752D0).
        #   - MomentHardStop rolls its ultra slo-mo on it (the first HIGH world crash, then every 3rd; x0.005..0.01).
        #     The fast top-down shot's ICE take (6.678 s) holds the frame for 401 updates.
        #   - The crash countdown only ticks SIM time (TickCrashes 0x827C6528), so the reset lands that much later.
        #   - At 40 s the gesture came right after the hold ended. Showtime then started from the wrecked car at rest:
        #     no world contact, no stompee.
        # How 49 was chosen: the worst hold measured on 3 runs + 2 s. Every run measured 6.7 s of wall time for the hold
        # and 10.6-10.7 s from crash to reset. At 49 the gesture lands 5.0-5.2 s after the reset (the analyser-off
        # runs had 3.0-3.3 s at 40), and all three passed.
        Showtime = '49'
        ShowtimeIgnoreProgression = $true
        # Fresh profiles play the junkyard introduction before car selection.
        MaxSeconds = 149
    }
    DiagEnv = 'BRN_SHOWTIME_WATCH=1,BRN_ROLL_PROBE=1,BRN_SLOMO_DIAG=1'
    Checks = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
        @{ Kind = 'LogMatch'; Name = 'Showtime physics entered';
           Pattern = '\[showtime-watch\] mbPlayerCarInShowtime -> 1'; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'Showtime physics updating';
           Pattern = '\[showtime-watch\] crashing=1 .*disable=0'; Expect = $true }
        @{ Kind = 'Script'; Name = 'Showtime world contact response ran'; Script = {
            param($ctx)
            $contacts = @($ctx.LogLines | Where-Object {
                $_ -match '\[showtime-contact\] world=1 magnitude=.* handler=ApplyShowtimeContactImpulse'
            })
            @{ Pass = $contacts.Count -gt 0;
               Detail = "$($contacts.Count) completed nonzero world-contact calls to ApplyShowtimeContactImpulse" }
        } }
        @{ Kind = 'Script'; Name = 'game survived the contact run'; Script = {
            param($ctx)
            $ending = ($ctx.MarksText -split "`n") | Where-Object { $_ -match '^EXIT\s+' } | Select-Object -First 1
            @{ Pass = ($ending -match 'harness-stop'); Detail = "$ending".Trim() }
        } }
    )
}
