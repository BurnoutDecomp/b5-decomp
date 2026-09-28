# L4 boot order (owner's list 2026-09-28) -- "The junkyard doesn't save the last vehicle we used for the default
# position". BOOT 1 of 2: drive a DIFFERENT car and let the autosave write the profile. BOOT 2 is
# tests/ProfileDeliveryLive.ps1 on the SAME slot: its OnProfileLoaded line must name this car, and the player must
# stream in in it.
#
# THE CONSOLE. Every player-car change stores the new car as the profile's spawn car (ProgressionManager::
# OnPlayerCarChange @0x8237AC38: Profile+0x50/+0x58, reached from GameStateModule::OnPlayerCarChange with
# lbUpdateProfile = 1); the autosave writes it; the next boot's OnProfileLoaded @0x82397310 (ProcessGameEvents case 8,
# the MemoryCard exit) enters the junkyard nearest the saved position with GetSpawnCar @0x823763C8's answer -- that
# car, unless the rank is below its unlock rank. On the PC the start-of-game one-shot ran after the Deserialise and
# overwrote the saved car with VehicleList[0] on every boot, and OnProfileLoaded had no body.
#
# HOW THE CAR IS CHANGED. BRN_DEBUG_PLAYER_CAR (GameStateModule::HarnessInjectPlayerCarBringUp, the development menu's
# own ChangePlayerCarEvent -> ProcessGameEvents case 2 -> HandleChangePlayerCarEvent @0x82397568 ->
# OnPlayerCarChange(car, wheel, queue, 1)), once, ~3 s after the junkyard exit. The junkyard CAROUSEL is not used:
# on this build it does not answer the harness's OptionNext taps (two runs, junkyard_last_car_pick/20260928_082158
# and _083807: the car name stayed P_US_CopIndy_02) -- a GUI-side gap, reported, not this case's subject.
#
# The witnesses: "[car] ***** HARNESS-ONLY PLAYER CAR SWAP", [one-profile] spawnCarId (sampled on change),
# "STRM: Adding racecar for streaming: car=0, model=", "[SaveLoadPC] profile container WRITTEN" (the autosave).
# NO FRAME DUMP. A returning-player save on a slot > 0 (a COPY, never the owner's own file).
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/JunkyardLastCarLive.ps1 -Slot <n>

$FirstLine = {
  param($lines, $pattern, $from)
  for ($n = [Math]::Max(0, $from); $n -lt $lines.Count; $n++) { if ($lines[$n] -match $pattern) { return $n } }
  return -1
}

@{
  Name    = 'junkyard_last_car'
  Area    = 'flow'
  Bug     = 'The car the player last drove must be the next boot''s junkyard car (the profile''s spawn car, saved by the autosave).'
  Frames  = $false
  Run     = @{
    Drive      = $true
    SkipIntro  = $true
    AcceptGap  = 1.0
    MaxSeconds = 110
  }
  DiagEnv = 'BRN_DEBUG_PLAYER_CAR=PUSCV01'
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the car was changed once (the development menu''s ChangePlayerCarEvent)'
       Pattern = '^\[car\] \*\*\*\*\* HARNESS-ONLY PLAYER CAR SWAP'; Min = 1; Max = 1 }
    @{ Kind = 'Script';   Name = 'after the change the player''s car-0 stream-in is the new car, the profile''s spawn car became it, and the autosave wrote the profile after that'; Script = {
        param($ctx)
        $lines = $ctx.LogLines
        $swap = & $FirstLine $lines '^\[car\] \*\*\*\*\* HARNESS-ONLY PLAYER CAR SWAP' 0
        if ($swap -lt 0) { return @{ Pass = $false; Detail = 'no swap line' } }
        $strm = & $FirstLine $lines '^STRM: Adding racecar for streaming: car=0, model=VEH_PUSCV01\s*,' $swap
        $spawnBefore = ''
        for ($n = $swap; $n -ge 0; $n--) { if ($lines[$n] -match '^\[one-profile\] .*spawnCarId gs=([0-9A-F]{16})') { $spawnBefore = $Matches[1]; break } }
        $changed = -1; $spawnAfter = ''
        for ($n = $swap; $n -lt $lines.Count; $n++) {
          if ($lines[$n] -match '^\[one-profile\] .*spawnCarId gs=([0-9A-F]{16})' -and $Matches[1] -ne $spawnBefore) { $changed = $n; $spawnAfter = $Matches[1]; break }
        }
        $written = if ($changed -ge 0) { & $FirstLine $lines '^\[SaveLoadPC\] profile container WRITTEN' $changed } else { -1 }
        $ok = ($strm -ge 0) -and ($changed -ge 0) -and ($written -ge 0)
        return @{ Pass = $ok; Detail = ("swap at log line {0}; VEH_PUSCV01 streamed at {1}; profile spawn car {2} -> {3} at {4}; autosave written at {5}" -f ($swap + 1), ($strm + 1), $spawnBefore, $spawnAfter, ($changed + 1), ($written + 1)) }
      }.GetNewClosure() }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
