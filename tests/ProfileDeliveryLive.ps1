# L4 boot order (owner's list 2026-09-28) -- the loaded profile reaches the game state at the MemoryCard exit, as on
# the console, and what it carries survives: the road-rule records, the car total, the saved car.
#
# THE CONSOLE ORDER. The start-of-game one-shot (SendSetupPlayerCarEvent) runs in the loading spine's first frame
# (LoadingScriptedState::Update @0x823F22D8 -> PreWorldUpdate @0x823F27BC). The profile is deserialised in the
# MemoryCard state; leaving that state posts game event 8, and ProcessGameEvents @0x823A0A18 case 8 runs
# `if (mbWaitForStreaming) { OnProfileLoaded(); WaitForStreaming(); }`. OnProfileLoaded @0x82397310 puts the save
# back into the running game: StreetManager::OnProfileLoaded copies the road-rule tables out of the profile (without
# it StreetManager::Update copies its ZEROED tables into the profile every frame, and the next save writes the
# wipe), ProgressionManager::OnLoadProfile re-derives the car total, and the junkyard entry uses the SAVED car.
# Unit tests: tests/run_profile_delivery.py, tests/run_boot_order.py.
#
# The witnesses: [GameStateModule::OnProfileLoaded] (one line per load: the saved car's name, the car total),
# [one-profile] (sampled on change: roadRules gs = the profile's road-rule rows holding a score, maxCars gs),
# "STRM: Adding racecar for streaming: car=0, model=VEH_<name>" (the car the world streams for the player).
# NO FRAME DUMP. Run it on a slot whose Memcard_<n> holds a returning player's save with road-rule records -- a
# COPY, never the owner's own file. Run it twice on the same slot: the second boot reads the save the first one's
# autosave wrote, so its roadRules line proves the records survived a save and a reboot.
#
# EXPECTED ON THE PRE-FIX EXE: FAIL -- no OnProfileLoaded line; roadRules drops to 0 after the first in-game frame.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/ProfileDeliveryLive.ps1 -Slot <n>

# The first line index at or after $from matching a pattern, or -1.
$FirstLine = {
  param($lines, $pattern, $from)
  for ($n = [Math]::Max(0, $from); $n -lt $lines.Count; $n++) { if ($lines[$n] -match $pattern) { return $n } }
  return -1
}

@{
  Name    = 'profile_delivery'
  Area    = 'flow'
  Bug     = 'The boot profile must reach the game state at the MemoryCard exit (ProcessGameEvents case 8 -> OnProfileLoaded @0x82397310): road-rule records kept, car total restored, the saved car spawned.'
  Frames  = $false
  Run     = @{
    Drive      = $true
    SkipIntro  = $true
    AcceptGap  = 1.0
    MaxSeconds = 100
  }
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'OnProfileLoaded ran exactly once at the boot'
       Pattern = '^\[GameStateModule::OnProfileLoaded\] '; Min = 1; Max = 1 }
    @{ Kind = 'Script';   Name = 'the console order: one-shot, MemoryCard state, Deserialise, OnProfileLoaded (event 8), then the in-game junkyard entry (case 78)'; Script = {
        param($ctx)
        $lines = $ctx.LogLines
        $shot  = & $FirstLine $lines '^\[GameStateModule::SendSetupPlayerCarEvent\] junkyard=' 0
        $card  = & $FirstLine $lines '^MemoryCard: OnEnter' 0
        $deser = & $FirstLine $lines '^\[profile-save\] deserialised' 0
        $load  = & $FirstLine $lines '^\[GameStateModule::OnProfileLoaded\] ' 0
        $jy    = & $FirstLine $lines '^\[GameStateModule::ProcessGameEvents case 78\] ReallyEnterJunkyardAtStartOfGame done' 0
        $ok = ($shot -ge 0) -and ($card -gt $shot) -and ($deser -gt $card) -and ($load -gt $deser) -and ($jy -gt $load)
        return @{ Pass = $ok; Detail = ("log lines: one-shot {0}, MemoryCard {1}, deserialised {2}, OnProfileLoaded {3}, case 78 {4}" -f ($shot + 1), ($card + 1), ($deser + 1), ($load + 1), ($jy + 1)) }
      }.GetNewClosure() }
    @{ Kind = 'Script';   Name = 'the road-rule records are loaded and never wiped (every [one-profile] roadRules value after the Deserialise equals the loaded count, which is > 0)'; Script = {
        param($ctx)
        $lines = $ctx.LogLines
        $deser = & $FirstLine $lines '^\[profile-save\] deserialised' 0
        if ($deser -lt 0) { return @{ Pass = $false; Detail = 'no Deserialise line' } }
        $values = @()
        for ($n = $deser; $n -lt $lines.Count; $n++) {
          if ($lines[$n] -match '^\[one-profile\] .*\| roadRules gs=(-?\d+)') { $values += [int]$Matches[1] }
        }
        if ($values.Count -eq 0) { return @{ Pass = $false; Detail = 'no [one-profile] roadRules sample after the Deserialise' } }
        $loaded = $values[0]
        $min = ($values | Measure-Object -Minimum).Minimum
        return @{ Pass = ($loaded -gt 0 -and $min -ge $loaded); Detail = ("loaded {0} road-rule row(s); {1} sample(s) after the Deserialise, min {2}, last {3}" -f $loaded, $values.Count, $min, $values[-1]) }
      }.GetNewClosure() }
    @{ Kind = 'Script';   Name = 'the car total is OnLoadProfile''s: the list-only count plus one per "Profile: increasing max car count" line the load printed (owned sponsor cars, CARBEAGT), and it stays there'; Script = {
        param($ctx)
        $lines = $ctx.LogLines
        $deser = & $FirstLine $lines '^\[profile-save\] deserialised' 0
        $load = & $FirstLine $lines '^\[GameStateModule::OnProfileLoaded\] ' 0
        if ($deser -lt 0 -or $load -lt 0) { return @{ Pass = $false; Detail = "deserialised $($deser + 1), OnProfileLoaded $($load + 1)" } }
        if ($lines[$load] -notmatch ' maxCars=(\d+)') { return @{ Pass = $false; Detail = 'no maxCars on the OnProfileLoaded line' } }
        $atLoad = [int]$Matches[1]
        $before = $null
        for ($n = $load - 1; $n -ge 0; $n--) { if ($lines[$n] -match '^\[one-profile\] .*\| maxCars gs=(-?\d+)') { $before = [int]$Matches[1]; break } }
        $bumps = 0
        for ($n = $deser; $n -lt $load; $n++) { if ($lines[$n] -match '^Profile: increasing max car count') { $bumps++ } }
        $later = @()
        for ($n = $load; $n -lt $lines.Count; $n++) { if ($lines[$n] -match '^\[one-profile\] .*\| maxCars gs=(-?\d+)') { $later += [int]$Matches[1] } }
        $ok = ($null -ne $before) -and ($atLoad -eq $before + $bumps) -and (@($later | Where-Object { $_ -ne $atLoad }).Count -eq 0)
        return @{ Pass = $ok; Detail = ("list-only {0} + {1} sponsor/CARBEAGT line(s) = {2} at OnProfileLoaded; later samples: {3}" -f $before, $bumps, $atLoad, $(if ($later.Count) { ($later | Select-Object -First 6) -join ',' } else { 'none (unchanged)' })) }
      }.GetNewClosure() }
    @{ Kind = 'Script';   Name = 'the player spawns in the saved car: the first car-0 stream-in after OnProfileLoaded is VEH_<spawnCar>'; Script = {
        param($ctx)
        $lines = $ctx.LogLines
        $load = & $FirstLine $lines '^\[GameStateModule::OnProfileLoaded\] ' 0
        if ($load -lt 0 -or $lines[$load] -notmatch ' spawnCar=(\S+) profileCar=(\S+)') { return @{ Pass = $false; Detail = 'no OnProfileLoaded car names' } }
        $spawn = $Matches[1]; $profileCar = $Matches[2]
        $strm = & $FirstLine $lines '^STRM: Adding racecar for streaming: car=0, model=' $load
        if ($strm -lt 0) { return @{ Pass = $false; Detail = "no car-0 stream-in after OnProfileLoaded (spawnCar=$spawn)" } }
        $ok = $lines[$strm] -match ('model=VEH_' + [regex]::Escape($spawn) + '\s*,')
        return @{ Pass = $ok; Detail = ("spawnCar={0} profileCar={1}; stream-in: {2}" -f $spawn, $profileCar, $lines[$strm].Substring(0, [Math]::Min(110, $lines[$strm].Length))) }
      }.GetNewClosure() }
    @{ Kind = 'LogCount'; Name = 'the junkyard entry completed in game (case 78 -> ReallyEnterJunkyardAtStartOfGame)'
       Pattern = '^\[GameStateModule::ProcessGameEvents case 78\] ReallyEnterJunkyardAtStartOfGame done'; Min = 1 }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
