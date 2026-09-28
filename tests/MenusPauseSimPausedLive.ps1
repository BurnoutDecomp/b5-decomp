# Director pause byte and first-resume regression. ARTIST DoUpdate_Director
# @0x823E8EF8..0x823E8F2C copies the offline sim-pause byte. MainDirector does
# not call the replay-only VehicleTracker::Construct reset at boot; its first
# unpaused tracker update appends one sample and cannot divide by a zero step.
# Include the real pause playlist and black-and-white visual checks as well.
$case = & (Join-Path $PSScriptRoot 'MenusPauseCameraLive.ps1')
$case.Name = 'menus_pause_simpaused'
$case.Bug = 'The director must receive the sim-pause byte without introducing zero-timestep tracker assertions at the first resume.'
$case.DiagEnv += ',BRN_FRAME_DUMP_MAX=160'
$case.Checks += @(
    @{ Kind='LogCount'; Name='the pause press paused the sim'; Pattern='\[sim-pause\] game event 93 payload 1 -> RequestPause\(4\)'; Min=1 }
    @{ Kind='LogCount'; Name='the sim resumed after the pause'; Pattern='\[sim-pause\] action 87 -> RESUMED'; Min=1 }
    @{ Kind='LogCount'; Name='the arbitrator received the paused sim byte'; Pattern='\[pause-cam\] arbitrator NORMAL -> CRASH_NAV_ICE_CAMERAS .* paused 1\)'; Min=1 }
    @{ Kind='LogCount'; Name='no zero-timestep tracker or Looker assertions'; Pattern='IsZero\((mTimestepJournal\[0\]|lTimeStep\.GetFloat\(\))\)'; Max=0 }
    @{ Kind='NewAsserts'; Name='no new assertion families, including boot and first resume'; Known=(Join-Path $PSScriptRoot 'MenusKnownAsserts.txt') }
)
$case
