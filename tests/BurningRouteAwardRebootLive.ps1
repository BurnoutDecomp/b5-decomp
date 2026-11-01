# Copy the preceding BurningRouteAwardLive run's Profile.sav.after to the named
# scratch fixture, then launch the same selected reward through a fresh process.
@{
  Name = 'burning_route_award_reboot'
  Area = 'gui/carselect'
  Bug = 'A save retaining the Burning Route reward must load, select and drive out again.'
  ProfileFixture = 'scratch/first_burning_route_after_selection.sav'
  Frames = $false
  Run = @{
    TitleStart = $true
    SkipIntro = $true
    Drive = $true
    MotionProbe = $true
    MaxSeconds = 65
    AcceptGap = 2.0
    ThrottleScript = '0:accel,8:none'
  }
  Checks = @(
    @{ Kind='LogCount'; Name='no assertions'; Pattern='\[ASSERT'; Max=0 }
    @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
    @{ Kind='LogMatch'; Name='saved reward restored'; Pattern='OnProfileLoaded\].*spawnCar=XUSM1B1 profileCar=XUSM1B1' }
    @{ Kind='LogMatch'; Name='reward vehicle streamed'; Pattern='car=0, model=VEH_XUSM1B1' }
    @{ Kind='Mark'; Name='reloaded save exits junkyard'; Phase='DRIVING' }
  )
}
