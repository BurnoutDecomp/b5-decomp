# FX-DIRECTOR (crash parity 2026-09-24) -- the stunt moment's signature-jump take, live.
#
# With the moment tick running, a jump whose authored camera is CUSTOM posts OnJumpStart with the cut
# index as the requested camera, and MomentPlayerStunt::Update @0x82272750 stages the ICE take
# "World_Signature_<cut>" by its resource id. The id is the name's CRC32 ZERO-extended
# (CgsResource::ID::HashString @0x828D84A8 returns through `clrldi r3, r11, 0x20`). The PC sign-extended
# it, so for jump 4030 (cut 305, CRC 0x9CA2368A) the take was never found, "lpIceTake != NULL" fired and
# the guid read faulted at address 8 (scratch/bugtest/runs/fxdirector_moment_tick/20260924_180957, where
# the free drive after the sweep happened to take this jump at 126 mph).
# The shot: one seat 40 m before jump 4030 on the line and at the speed that run's free drive took --
# (3026.55, -8.9, -294.9), heading 1.45 deg, 55.8 m/s (its [motion] n 8040 sample).
# The tick is unconditional since 0d4289ce (the BRN_MOMENT_TICK opt-in the pre-commit proof run used is
# gone). BRN_CRASHCAM_DIAG=1 arms the NewMoment allocation line checked below (default off since the
# 2026-09-24 diag-hygiene pass).
@{
  Name    = 'fxdirector_stunt_jump'
  Area    = 'director'
  Bug     = 'A signature jump with the moment tick on must stage its World_Signature ICE take (found, no assert, no access violation).'
  Frames  = $false
  Run     = @{
    Drive            = $true
    MotionProbe      = $true
    MaxSeconds       = 60
    SkipIntro        = $true
    AcceptGap        = 1.0
    CrashSweep       = '3026.55,-8.9,-294.9'
    CrashSweepShots  = '3026.55/-8.9/-294.9/1.45:55.8'
    CrashSweepSettle = 600
  }
  DiagEnv = 'BRN_CRASHCAM_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'the sweep fired (the car was seated on the approach)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the jump posted its CUSTOM camera request (OnJumpStart, type 1)'
       Pattern = '\[jump-ladder\] action=56 OnJumpStart posted cut=\d+ type=1'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the moment factory allocated the pools (NewMoment)'
       Pattern = '\[jump-ladder\] MomentController::NewMoment allocated type=\d+ paramID=\d+ allocated=1'; Min = 3 }
    @{ Kind = 'LogCount';   Name = 'the stunt take was found (no "lpIceTake != NULL")'; Pattern = 'lpIceTake != NULL'; Max = 0 }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
