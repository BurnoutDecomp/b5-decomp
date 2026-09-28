# L4 boot order (owner's list 2026-09-28) -- "The junkyard doesn't save the last vehicle we used for the default
# position". BOOT 2 of 2: run it on the SAME slot right after tests/JunkyardLastCarLive.ps1, which drove PUSCV01 and
# let the autosave write it. The profile loaded at the MemoryCard exit (ProcessGameEvents case 8 -> OnProfileLoaded
# @0x82397310) must spawn the player in PUSCV01 at the junkyard: GetSpawnCar @0x823763C8 keeps the saved car (the
# player's rank is not below its unlock rank), CarSelectManager::EnterJunkyardAtStartOfGame @0x82393080 makes it the
# start car, and SpawnInStartCar streams it in. On the pre-fix exe this boot spawns the default PUSMC01.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/JunkyardLastCarRebootLive.ps1 -Slot <n>

$FirstLine = {
  param($lines, $pattern, $from)
  for ($n = [Math]::Max(0, $from); $n -lt $lines.Count; $n++) { if ($lines[$n] -match $pattern) { return $n } }
  return -1
}

@{
  Name    = 'junkyard_last_car_reboot'
  Area    = 'flow'
  Bug     = 'After a reboot the junkyard car must be the car the player last drove (PUSCV01, saved by the previous boot''s autosave).'
  Frames  = $false
  Run     = @{
    Drive      = $true
    SkipIntro  = $true
    AcceptGap  = 1.0
    MaxSeconds = 70
  }
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the loaded profile''s car is PUSCV01 and GetSpawnCar keeps it (OnProfileLoaded, the MemoryCard exit)'
       Pattern = '^\[GameStateModule::OnProfileLoaded\] .* spawnCar=PUSCV01 profileCar=PUSCV01 '; Min = 1; Max = 1 }
    @{ Kind = 'Script';   Name = 'the player spawns in it: the first car-0 stream-in after OnProfileLoaded is VEH_PUSCV01, and the default car is never streamed for car 0'; Script = {
        param($ctx)
        $lines = $ctx.LogLines
        $load = & $FirstLine $lines '^\[GameStateModule::OnProfileLoaded\] ' 0
        if ($load -lt 0) { return @{ Pass = $false; Detail = 'no OnProfileLoaded line' } }
        $strm = & $FirstLine $lines '^STRM: Adding racecar for streaming: car=0, model=' $load
        if ($strm -lt 0) { return @{ Pass = $false; Detail = 'no car-0 stream-in after OnProfileLoaded' } }
        $default = & $FirstLine $lines '^STRM: Adding racecar for streaming: car=0, model=VEH_PUSMC01\s*,' 0
        $ok = ($lines[$strm] -match 'model=VEH_PUSCV01\s*,') -and ($default -lt 0)
        return @{ Pass = $ok; Detail = ("first car-0 stream-in: {0}; a VEH_PUSMC01 car-0 stream-in at line {1}" -f $lines[$strm].Substring(0, [Math]::Min(90, $lines[$strm].Length)), ($default + 1)) }
      }.GetNewClosure() }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
