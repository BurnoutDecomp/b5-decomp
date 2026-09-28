# L6 AIDRIVE (owner list 2026-09-27) -- "The enemies AIs always crash to the traffic or to walls": the AI TRAFFIC FEED, live.
#
# TrafficEntityModule::StoreAISceneResultsForNextFrame @0x82728400 had no body and its PrePhysicsUpdate call (0x8274C804)
# was a logged park, so maStoredAITrafficData never held a traffic id, ConvertSceneResultsToTrafficDataForAI never published
# a TrafficAIEntity, and every rival's avoidance list (AIModule::SortTrafficIntoAICars) held race cars only: the steering
# fan's AvoidTraffic / AvoidOncomingTraffic rows (IncludeConstantBearing, kfBias -100 / -400) and the ProximitySpeed brake
# never saw a traffic car. This case races the five rivals through city traffic and checks the feed end to end.
#
# The race is FxDirectorCheckpointLive.ps1's (junction 480886, event mode 0), driven by the game's own AI on the pad:
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxAiDriveTrafficFeedLive.ps1 --ai-pad race --no-frames --slot <n>
# Witnesses (PC only, default off; BrnAIModule_Drive.cpp):
#   [ai-traffic] entities E drivers s0=n s1=n ...   once a second: traffic entities published to the AI, and each active
#                                                     driver's traffic entries (the dispatch proof)
#   [ai-crash] slot ... rows traffic .. oncoming .. hngAvoid .. hngExit .. edges .. nearestTraffic ..   at each AI crash onset
# RED on an exe without the drain (d65db9997047): the "[Q7-traffic-leg] ... StoreAISceneResultsForNextFrame" park line is
# printed, no [ai-traffic] line shows a rival with traffic, and [rival] near never exceeds the five other race cars.
$case = & (Join-Path $PSScriptRoot 'FxDirectorCheckpointLive.ps1')
$case.Name = 'fxaidrive_traffic_feed'
$case.Area = 'ai'
$case.Bug = 'The traffic module must drain the per-race-car AI frustum queries (StoreAISceneResultsForNextFrame @0x82728400) so the AI avoids traffic.'
$case.Run.MaxSeconds = 130
$case.DiagEnv += ',BRN_AI_TRAFFIC_DIAG=1,BRN_AI_CRASH_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the race started'; Pattern = '\[ai-evt\] action 34 START_PLAYING_MODE'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the drain is no longer parked (0x8274C804)'
       Pattern = '\[Q7-traffic-leg\].*StoreAISceneResultsForNextFrame'; Max = 0 }
    @{ Kind = 'LogValue'; Name = 'the traffic module published traffic entities to the AI'
       Pattern = '\[ai-traffic\] entities (?<n>\d+)'; Group = 'n'; Agg = 'max'; Min = 1 }
    @{ Kind = 'Script'; Name = 'rival avoidance lists held traffic cars (a rival slot with traffic entries)'; Script = {
        param($ctx)
        $lines = 0; $withRivalTraffic = 0; $maxRival = 0; $sample = ''
        foreach ($line in $ctx.LogLines) {
            if ($line -notmatch '^\[ai-traffic\] entities (\d+) drivers(.*)\[FLAG') { continue }
            $lines++
            $hit = $false
            foreach ($m in [regex]::Matches($Matches[2], ' s([1-7])=(\d+)')) {
                $n = [int]$m.Groups[2].Value
                if ($n -gt 0) { $hit = $true; if ($n -gt $maxRival) { $maxRival = $n; $sample = $line.Trim() } }
            }
            if ($hit) { $withRivalTraffic++ }
        }
        @{ Pass = ($withRivalTraffic -gt 0)
           Detail = ("{0} of {1} [ai-traffic] samples had a rival holding traffic; most in one rival's list {2}; e.g. {3}" -f $withRivalTraffic, $lines, $maxRival, $sample) }
    } }
    @{ Kind = 'LogCount'; Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
)
$case
