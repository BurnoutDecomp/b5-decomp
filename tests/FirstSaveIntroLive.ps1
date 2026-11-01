@{
  Name = 'first_save_intro'
  Area = 'gui/progression'
  Bug = 'A first save must play the post-title montage and reach driving without GUI assertions.'
  FreshProfile = $true
  Frames = $true
  Run = @{
    MaxSeconds = 240
    AcceptGap = 9999.0
    FrameEvery = 120
    MotionProbe = $true
    Drive = $true
    ThrottleScript = '0:accel,8:none'
  }
  Setup = {
    param($ctx)
    $helper = Join-Path $ctx.Root 'b5-decomp/tests/run_first_save_input.ps1'
    Start-Process pwsh -WindowStyle Hidden -PassThru -ArgumentList @(
      '-NoProfile','-File',('"'+$helper+'"'),'-GameLog',('"'+$ctx.GameLog+'"'),'-Slot',$ctx.Slot
    ) -RedirectStandardOutput (Join-Path $ctx.RunDir 'input.log') -RedirectStandardError (Join-Path $ctx.RunDir 'input.err')
  }
  Checks = @(
    @{ Kind='LogCount'; Name='no assertions'; Pattern='\[ASSERT'; Max=0 }
    @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
    @{ Kind='LogMatch'; Name='intro movie opened'; Pattern="\[Movie\] prepared VIDEOS\\intro.vp6" }
    @{ Kind='LogMatch'; Name='learner permit and flyby completed'; Pattern='\[Intro\] state 9 -> 3' }
    @{ Kind='Mark'; Name='junkyard exit completed'; Phase='DRIVING' }
  )
}
