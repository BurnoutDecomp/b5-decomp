# FX-AIBUZZ item 3 (crash parity 2026-09-24) live check: GameStateModule's start-of-game latch (+0x32DC4,
# mbIsFirstUpdate == mbSendSetupPlayerCarPending) is armed in Construct, where the console arms it
# (`stbx r24(=1), r31, 0x32DC4` @0x823807A4), instead of at the end of Prepare's terminal stage.
# The claim to show: NOTHING OBSERVABLE MOVES. The latch's only consumer, PreWorldUpdateSetupPlayerCarBringUp,
# runs only in E_MGS_IN_GAME, so the one-shot leg (SendSetupPlayerCarEvent + SendSetUpAllEventStartsMessage) must
# still fire EXACTLY ONCE, after Prepare is done, and before the GUI's game event 78 completes the junkyard entry --
# the same sequence every earlier run logs (e.g. fxaibuzz_place/20260924_220901: prepare DONE at line 314, the
# setup event at 1304, event starts at 1306, case 78 at 1464) -- and the car must still drive.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxAiBuzzFirstUpdateLive.ps1
@{
    Name = 'fxaibuzz_first_update'
    Area = 'gamestate/flow'
    Bug = 'GameStateModule arms its start-of-game latch (mbIsFirstUpdate +0x32DC4) in Construct like the console; the one-shot setup leg must still fire exactly once, in-game, after Prepare.'
    Frames = $false
    Run = @{
        Drive = $true
        MotionProbe = $true
        SkipIntro = $true
        AcceptGap = 1.0
        SkipTrainingTip = $true
        MaxSeconds = 60
    }
    Checks = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
        @{ Kind = 'LogCount'; Name = 'the one-shot setup leg fired exactly once (SendSetupPlayerCarEvent)'
           Pattern = '\[GameStateModule::SendSetupPlayerCarEvent\] junkyard='; Min = 1; Max = 1 }
        @{ Kind = 'LogCount'; Name = 'its partner published the event starts exactly once'
           Pattern = '\[event-starts\] published \d+ event starts'; Min = 1; Max = 1 }
        @{ Kind = 'LogMatch'; Name = 'the junkyard entry completed (game event 78)'
           Pattern = '\[GameStateModule::ProcessGameEvents case 78\] ReallyEnterJunkyardAtStartOfGame done'; Expect = $true }
        @{ Kind = 'Script'; Name = 'order: Prepare DONE -> setup leg -> event starts -> case 78'; Script = {
            param($ctx)
            $find = { param($re) for ($i = 0; $i -lt $ctx.LogLines.Count; $i++) { if ($ctx.LogLines[$i] -match $re) { return $i } }; return -1 }
            $prep  = & $find '\[GameStateModule::Prepare\] stage 26 .*prepare DONE'
            $setup = & $find '\[GameStateModule::SendSetupPlayerCarEvent\] junkyard='
            $evts  = & $find '\[event-starts\] published \d+ event starts'
            $c78   = & $find '\[GameStateModule::ProcessGameEvents case 78\] ReallyEnterJunkyardAtStartOfGame done'
            $ok = ($prep -ge 0) -and ($setup -gt $prep) -and ($evts -gt $setup) -and ($c78 -gt $evts)
            return @{ Pass = $ok; Detail = ('lines: prepare DONE {0}, setup {1}, event starts {2}, case 78 {3}' -f ($prep + 1), ($setup + 1), ($evts + 1), ($c78 + 1)) }
          } }
    )
}
