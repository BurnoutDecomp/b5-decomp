# FX-DIRECTOR2 (crash parity 2026-09-25) -- live witness for the world -> director traffic hop and the team words.
#
# BrnGameModule::BridgeWorldToDirector step 4 (0x823E3F5C..0x823E3F78) copies the world output's
# TrafficDirectorOutputInterface into the director input at +0x6AC0; MainDirector::PreSceneQueryUpdate hands its
# entity array to AllVehicleData::Update @0x8221D938 as lpTrafficVehicleArray, with the input's eight team words
# (+0x3238, written by BridgeGameStateToDirector's SetVehicleTeam loop 0x823CD514..0x823CD56C from
# OnlineScoringOutputInterface::maePlayerTeam) as lpaVehicleTeams. Before the fix the director input carried the
# span as opaque bytes, step 4 was dropped and every nearest-car row carried team 0 by fiat.
#
# The drive is RivalOrganic.ps1's Road Rage pursuit: an event with rivals (race-car rows beyond the player's) that
# shoves and rear-ends plenty of traffic. Witnesses (diagnostics only print):
#   BRN_DIRECTOR_TRAFFIC_DIAG  `[director-traffic] frame F AllVehicleData traffic N first{...} entities e0,e1,..
#                              rows car0:teamT car1:teamT ...` -- what AllVehicleData::Update stored, on every
#                              traffic-count change and every 300th frame (600 lines at most).
#   BRN_CRASH_ACTION_DIAG      `[traffic-crash] added vehicle=V owner=O` -- CrashModule's traffic crash record;
#                              V is the traffic vehicle index, the same index TrafficEntityModule writes into each
#                              director record's mu16EntityIndex.
# Offline nothing writes OnlineScoringOutputInterface::maePlayerTeam -- its only writer is the online scorers'
# WriteDataToOutput (BrnBaseOnlineModeScoring.cpp), and GameStateModuleIO::OutputBuffer::Construct @0x82382940 clears the
# scoring snapshot (memset +0x2A4B8, 0xAB0) and sets maOnlineAwards to -1 but leaves the team words -- and nothing reads
# the rows' teams offline either: both queries' only caller is ArbStateRoaming::ProcessPossibleFX's online arms 12 / 14
# / 17 (0x82234A00). So the offline teams are the untouched IO-buffer words, one constant value; the check reports it.
# The numbers are tests/run_fxdirector2_traffic_hop.py's.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxDirector2TrafficHopLive.ps1 --run-name fxdirector2_traffic_hop
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxdirector2_traffic_hop'
$case.Area = 'director'
$case.Bug = 'The director must receive the traffic records (BridgeWorldToDirector step 4) and the per-car teams, and AllVehicleData::Update must store both (0x8221D938).'
$case.DiagEnv += ',BRN_DIRECTOR_TRAFFIC_DIAG=1,BRN_CRASH_ACTION_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogValue'; Name = 'AllVehicleData receives traffic records (N > 0)';
       Pattern = '\[director-traffic\] frame \d+ AllVehicleData traffic (?<n>\d+)'; Group = 'n'; Agg = 'max'; Min = 1 }
    @{ Kind = 'Script'; Name = 'the race-car rows: the rivals are listed, all with one offline team value in the EPlayerTeam range'; Script = {
        param($ctx)
        $most = 0; $lines = 0; $sample = ''; $teams = @{}
        foreach ($l in $ctx.LogLines) {
            if ($l -notmatch '\[director-traffic\] .* rows(?<rows>( car\d+:team-?\d+)*)\s*$') { continue }
            $lines++
            $rows = @([regex]::Matches($Matches['rows'], 'car(\d+):team(-?\d+)'))
            if ($rows.Count -gt $most) { $most = $rows.Count; $sample = $Matches['rows'].Trim() }
            foreach ($r in $rows) { $teams[[int]$r.Groups[2].Value] = $true }
        }
        $values = @($teams.Keys | Sort-Object)
        $inRange = @($values | Where-Object { $_ -ge 0 -and $_ -lt 9 }).Count -eq $values.Count
        @{ Pass = ($most -ge 2) -and ($values.Count -eq 1) -and $inRange;
           Detail = "$lines line(s); most rows on one line $most ($sample); team value(s) seen: $($values -join ', ')" }
    } }
    @{ Kind = 'Script'; Name = 'a crashed traffic car is among the records AllVehicleData holds at that moment'; Script = {
        param($ctx)
        # The director's list is the traffic around the PLAYER; a race car's traffic crash there (the player's or a
        # rival's) matches the director line printed last before it, or the next one (lines print on count changes).
        $last = @(); $lastTag = ''; $pending = @(); $crashes = 0; $byPlayer = 0; $matched = 0; $detail = @()
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[director-traffic\] frame (\d+) AllVehicleData traffic (\d+)') {
                $frame = [int]$Matches[1]; $n = [int]$Matches[2]
                $ents = @()
                if ($l -match ' entities ([\d,]+)') { $ents = @($Matches[1] -split ',' | ForEach-Object { [int]$_ }) }
                foreach ($p in $pending) {
                    if ($ents -contains $p.V) { $matched++; $detail += "vehicle $($p.V) (owner $($p.O)) in the next list (frame $frame, N=$n)" }
                }
                $pending = @()
                $last = $ents; $lastTag = "frame $frame, N=$n"
                continue
            }
            if ($l -match '\[traffic-crash\] added vehicle=(\d+) owner=(\d+)') {
                $crashes++
                $v = [int]$Matches[1]; $o = [int]$Matches[2]
                if ($o -eq 0) { $byPlayer++ }
                if ($last -contains $v) { $matched++; $detail += "vehicle $v (owner $o) in the list of $lastTag" }
                else { $pending += @{ V = $v; O = $o } }
            }
        }
        @{ Pass = ($matched -ge 1);
           Detail = "$crashes traffic crash(es) ($byPlayer by the player); $matched matched a director record: $(($detail | Select-Object -First 5) -join '; ')" }
    } }
)
$case
