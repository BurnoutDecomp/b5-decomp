# OWNERLIST 2026-09-27, lane L5 MENUS -- the director is told the sim is paused (the pause-time asserts).
#
# The owner: "There are still some asserts and crashes". L1's assert sweep of the pause (menus_pause_camera,
# exe d65db9997047, 20260927_213008): "!rw::math::fpu::IsZero(lTimeStep.GetFloat())" at BrnLooker.cpp:282 x1202,
# once a frame for the whole pause, from BehaviourBystanderCam::Update <- BehaviourManager::UpdateAllBehaviours <-
# MainDirector::PreSceneQueryUpdate.
#
# The console chain: the pause press -> GUI 191{activate 0} -> game event 93 payload 1 -> RequestPause(4) -> game action
# 86 -> BrnGameModule::CheckGameActions sets mbSimPaused and stops the sim timer; then every frame
# BrnGameModule::DoUpdate_Director @0x823E8DE0 stages it for the director (0x823E8EF8..0x823E8F2C):
#     input->SetSimPaused((updateSet & 0x100) ? 0 : (mbOnline ? 0 : mbSimPaused))
# and UpdateAllBehaviours @0x82251960 holds every behaviour that lacks the update-during-pause bit while it is set.
# The PC never staged the byte, so the director updated its behaviours on the stopped clock's zero timestep.
#
# The run: a returning boot on slot 6, the car NOT driven, the Driver Details pause at DRIVING+20 s, out at +40 s.
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/MenusPauseSimPausedLive.ps1 -Slot 6
@{
  Name    = 'menus_pause_simpaused'
  Area    = 'gui'
  Bug     = 'The director never saw the sim paused (DoUpdate_Director did not stage InputBuffer::mbSimPaused), so every behaviour updated through the pause on a zero timestep and the Looker asserted every frame.'
  Frames  = $false
  Run     = @{
    SkipIntro    = $true
    AcceptGap    = 1.0
    MaxSeconds   = 110
    PauseAt      = '20'
    PauseTarget  = 'driver'
    UnpauseAt    = '40'
  }
  DiagEnv = 'BRN_SCREEN_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the Driver Details pause screen opened (CN_D_DETAIL)'; Pattern = "\[screen\] ENTER 'CN_D_DETAIL\s*'"; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the pause press paused the sim (game event 93 payload 1 -> RequestPause(4))'
       Pattern = '\[sim-pause\] game event 93 payload 1 -> RequestPause\(4\)'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the sim was resumed after the pause (action 87)'; Pattern = '\[sim-pause\] action 87 -> RESUMED'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'no zero-timestep Looker asserts (BrnLooker.cpp:282) -- the director holds its behaviours through the pause'
       Pattern = 'IsZero\(lTimeStep\.GetFloat\(\)\)'; Max = 0 }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families (Niaz''s "Invalid suspension type" on START is known)'
       Known = (Join-Path $PSScriptRoot 'MenusKnownAsserts.txt') }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
