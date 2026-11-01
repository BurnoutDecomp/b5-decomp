# Prepare with create_burning_route_award_fixture.py, then run through run_case.ps1 -Slot <n>.
# This is a pending-award save fixture, not evidence of a driven/scored Burning Route.
@{
  Name = 'burning_route_award_selection'
  Area = 'gui/carselect'
  Bug = 'The reward ticker must preserve the junkyard type through vehicle/livery selection and exit.'
  ProfileFixture = 'scratch/first_burning_route_pending.sav'
  Frames = $false
  Run = @{
    TitleStart = $true
    SkipIntro = $true
    Drive = $true
    MotionProbe = $true
    MaxSeconds = 100
    AcceptGap = 2.0
    ThrottleScript = '0:accel,8:none'
  }
  DiagEnv = 'BRN_CARSEL_DIAG=1,BRN_SCREEN_DIAG=1'
  Checks = @(
    @{ Kind='LogCount'; Name='no assertions'; Pattern='\[ASSERT'; Max=0 }
    @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
    @{ Kind='LogMatch'; Name='pending reward revealed'; Pattern='Starting unlock for Normal' }
    @{ Kind='LogMatch'; Name='reward vehicle streamed'; Pattern='car=0, model=VEH_XUSM1B1' }
    @{ Kind='LogMatch'; Name='livery screen keeps junkyard type'; Pattern='CSL : Entering Car Select - 1' }
    @{ Kind='Mark'; Name='exited junkyard'; Phase='DRIVING' }
    @{ Kind='Script'; Name='reward is the saved player car'; Script={
      param($ctx)
      $path = Join-Path $ctx.RunDir 'Profile.sav.after'
      if (!(Test-Path -LiteralPath $path)) { return @{ Pass=$false; Detail='No post-run profile' } }
      $bytes = [IO.File]::ReadAllBytes($path)
      $spawn = [BitConverter]::ToUInt64($bytes, 312+80)
      $award = [uint64]15453815497113600000 # CgsIDCompress("XUSM1B1")
      @{ Pass=($spawn -eq $award); Detail="saved spawn=$spawn expected=$award" }
    } }
  )
}
