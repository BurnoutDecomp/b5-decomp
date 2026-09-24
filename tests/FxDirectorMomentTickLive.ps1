# FX-DIRECTOR (crash parity 2026-09-24) -- the crash HIGHLIGHT MOMENTS run live.
#
# MainDirector::Update calls UpdateMoments at 0x82274348 (the moment tick), right after
# UpdateCameraBehavioursPostScene. The PC held that call back: its first live run AV'd in
# BehaviourHelper::Prepare @0x82255F48, because the two behaviours the moments pool
# (BehaviourBystanderCam, BehaviourFixedCam) were hollow shells with no vtable (b5 22091bb2 /
# c65dea67 made them real Camera::Behaviours; 8d9b0b73 constructs the passenger block).
# With the tick running, a crash gets a highlight camera: the crash arbitrator state films it
# through the selected moment's camera instead of the failsafe gameplay camera, and returning to the
# gameplay camera raises the racing-gameplay flag again -- the edge the reset-on-track sting posts on.
#
# Shots (the FX-CRASHSND reset-sting sweep): the harbour rock-slope seat that drives into the water, a
# 44 m/s run into the quay rocks (a crash, then a reset), and the slope seat again.
# Witnesses:
#   [jump-ladder] MomentController::NewMoment allocated type=N   -- the factory filled the pools
#   [jump-ladder] MomentSelector muValidMoments=N                 -- a moment went VALID (one-shot)
#   [crashcam] crash camera: moment type=N valid=1 ...            -- BRN_CRASHCAM_DIAG: the crash state
#                                                                    films through a MOMENT's camera
#   [fx-sting-post] reset-on-track #N update=U                    -- BRN_HUD_SOUND_DIAG
# and the run must be clean: 0 asserts, 0 exceptions.
# BRN_MOMENT_TICK=1 is the default-OFF opt-in the lane's pre-commit proof run used; once the call is
# un-gated the variable is ignored and the same case runs unchanged.
@{
  Name    = 'fxdirector_moment_tick'
  Area    = 'director'
  Bug     = 'The crash highlight moments must run: a crash is filmed by a highlight moment camera, not only by the failsafe gameplay camera.'
  Frames  = $false
  Run     = @{
    Drive            = $true
    MotionProbe      = $true
    MaxSeconds       = 110
    SkipIntro        = $true
    AcceptGap        = 1.0
    CrashSweep       = '1758.5,-0.8,-2399.6'
    CrashSweepShots  = '1758.5/-0.8/-2399.6/262:12,1790.5/-3.2/-2393.0/260:44,1758.5/-0.8/-2399.6/262:12'
    CrashSweepSettle = 300
  }
  DiagEnv = 'BRN_HUD_SOUND_DIAG=1,BRN_WRECK_LATCH_DIAG=1,BRN_CRASHCAM_DIAG=1,BRN_MOMENT_TICK=1'
  Checks  = @(
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'the sweep fired (the car was placed)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the moment factory allocated the pools (NewMoment)'
       Pattern = '\[jump-ladder\] MomentController::NewMoment allocated type=\d+ paramID=\d+ allocated=1'; Min = 3 }
    @{ Kind = 'LogCount';   Name = 'a moment went VALID (the tick runs: MomentSelector muValidMoments > 0)'
       Pattern = '\[jump-ladder\] MomentSelector muValidMoments=[1-9]'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the crash arbitrator state ran (BRN_CRASHCAM_DIAG)'
       Pattern = '^\[crashcam\] '; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'a crash was filmed through a highlight MOMENT camera (type >= 0, valid)'
       Pattern = '^\[crashcam\] crash camera: moment type=\d+ valid=1 '; Min = 1 }
    @{ Kind = 'Script';     Name = 'the moment camera dropped the racing-gameplay flag while it filmed'; Script = {
        param($ctx)
        $moment = @(); $bad = @()
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[crashcam\] crash camera: moment type=(?<t>\d+) valid=(?<v>\d) validMoments=(?<n>\d+) racingGameplayFlag=(?<f>\d)') {
            $moment += ('type {0} (valid {1}, {2} valid, flag {3})' -f $Matches.t, $Matches.v, $Matches.n, $Matches.f)
            if ($Matches.f -ne '0') { $bad += $Matches.t }
          }
        }
        if ($moment.Count -eq 0) { return @{ Pass = $false; Detail = 'no moment camera filmed a crash' } }
        return @{ Pass = ($bad.Count -eq 0); Detail = (($moment -join '; ') + $(if ($bad.Count) { ' -- FLAG STILL SET for ' + ($bad -join ',') } else { '' })) }
      } }
    @{ Kind = 'LogCount';   Name = 'the reset-on-track sting was posted (the flag rose back on the gameplay camera)'
       Pattern = '^\[fx-sting-post\] reset-on-track #\d+ update=\d+'; Min = 1 }
    @{ Kind = 'Script';     Name = 'no reset-on-track post on two consecutive CameraControl updates'; Script = {
        param($ctx)
        $updates = @()
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[fx-sting-post\] reset-on-track #\d+ update=(?<u>\d+)') { $updates += [int64]$Matches.u }
        }
        if ($updates.Count -eq 0) { return @{ Pass = $false; Detail = 'no [fx-sting-post] line' } }
        $consecutive = 0
        for ($i = 1; $i -lt $updates.Count; $i++) { if (($updates[$i] - $updates[$i - 1]) -le 1) { $consecutive++ } }
        return @{ Pass = ($consecutive -eq 0); Detail = ('{0} posts at updates [{1}]; consecutive pairs {2}' -f $updates.Count, ($updates -join ','), $consecutive) }
      } }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
