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
        Showtime = '40'
        ShowtimeIgnoreProgression = $true
        # Fresh profiles play the junkyard introduction before car selection.
        MaxSeconds = 140
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
