# L4 boot order (2026-09-28) -- the start-of-game one-shot runs in the first loading-scripted frame, before the saved
# profile is deserialised, and the boot still reaches the junkyard and DRIVING.
#
# THE CONSOLE ORDER. Below load stage 8, LoadingScriptedState::Update @0x823F22D8 runs its partial spine, and that
# spine calls GameStateModule::PreWorldUpdate @0x823A5328 on every frame (bl @0x823F27BC). PreWorldUpdate's first
# call consumes the one-shot GameStateModule::Construct arms (gsm+208324): SendSetupPlayerCarEvent @0x8239A918 and
# SendSetUpAllEventStartsMessage. So it runs in the first frame of the first loading-scripted state -- long before
# the MemoryCard state deserialises the saved profile. The PC ran it only in E_MGS_IN_GAME, after the Deserialise.
# The junkyard car itself is placed in game, by case 78 -> ReallyEnterJunkyardAtStartOfGame -> SpawnInStartCar.
# Unit test: tests/run_boot_order.py.
#
# The witnesses are existing log lines: [GameStateModule::SendSetupPlayerCarEvent] (the one-shot),
# "MemoryCard: OnEnter" (the flow state), [profile-save] deserialised (the Deserialise), [GameStateModule::
# ProcessGameEvents case 78] (the in-game junkyard entry), [event-starts] published (the one-shot's partner call).
# NO FRAME DUMP. A returning-player boot: run it on a slot whose Memcard_<n> holds a save.
#
# EXPECTED ON THE PRE-FIX EXE: FAIL -- the one-shot line comes after "InGame: OnEnter" and after the Deserialise.
# ON THE FIXED EXE: every check passes.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/BootOrderLive.ps1 -Slot <n>

# The first line index matching a pattern, or -1.
$FirstLine = {
  param($lines, $pattern)
  for ($n = 0; $n -lt $lines.Count; $n++) { if ($lines[$n] -match $pattern) { return $n } }
  return -1
}

@{
  Name    = 'boot_order'
  Area    = 'flow'
  Bug     = 'The start-of-game one-shot (SendSetupPlayerCarEvent) must run in the loading spine''s first frame, before the saved profile is deserialised (LoadingScriptedState::Update @0x823F22D8 -> PreWorldUpdate @0x823F27BC).'
  Frames  = $false
  Run     = @{
    Drive      = $true
    SkipIntro  = $true
    AcceptGap  = 1.0
    MaxSeconds = 90
  }
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the start-of-game one-shot ran exactly once'
       Pattern = '^\[GameStateModule::SendSetupPlayerCarEvent\] junkyard='; Min = 1; Max = 1 }
    @{ Kind = 'Script';   Name = 'it ran in a loading-scripted state before the MemoryCard state and before the saved profile was deserialised (the pre-fix exe runs it in game)'; Script = {
        param($ctx)
        $lines = $ctx.LogLines
        $shot = & $FirstLine $lines '^\[GameStateModule::SendSetupPlayerCarEvent\] junkyard='
        $card = & $FirstLine $lines '^MemoryCard: OnEnter'
        $deser = & $FirstLine $lines '^\[profile-save\] deserialised'
        $ingame = & $FirstLine $lines '^InGame: OnEnter'
        if ($shot -lt 0) { return @{ Pass = $false; Detail = 'no one-shot line' } }
        $ok = ($card -lt 0 -or $shot -lt $card) -and ($deser -lt 0 -or $shot -lt $deser) -and ($ingame -lt 0 -or $shot -lt $ingame)
        return @{ Pass = $ok; Detail = ("one-shot at log line {0}; MemoryCard OnEnter {1}; deserialised {2}; InGame OnEnter {3}" -f ($shot + 1), ($card + 1), ($deser + 1), ($ingame + 1)) }
      }.GetNewClosure() }
    @{ Kind = 'LogValue'; Name = 'its partner call published the event-start table (SendSetUpAllEventStartsMessage)'
       Pattern = '^\[event-starts\] published (?<n>\d+) event starts'; Group = 'n'; Agg = 'max'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the junkyard entry completed in game (case 78 -> ReallyEnterJunkyardAtStartOfGame)'
       Pattern = '^\[GameStateModule::ProcessGameEvents case 78\] ReallyEnterJunkyardAtStartOfGame done'; Min = 1 }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
