@{
 Name='world_geometry_streaming'
 Area='rendering'
 Bug='World geometry must remain intact across camera and streamed-area changes.'
 Frames=$true
 Run=@{
  Drive=$true; SkipIntro=$true; AcceptGap=1.0; SkipTrainingTip=$true
  MotionProbe=$true; CrashPlayer=2500; MaxSeconds=100; FrameEvery=30; MinFreeGB=2
  CrashSweep='855.13,0.03,-2222.8'
  CrashSweepShots='855.13/0.03/-2222.8/90:0,3040.7/-2.8/-1937.9/180:0,855.13/0.03/-2222.8/270:0,1900/-4/-2390/270:0,855.13/0.03/-2222.8/90:0'
  CrashSweepSettle=600; CrashSweepMax=720; CrashSweepArm=0.1
 }
 DiagEnv='BRN_WORLD_GEOMETRY_DIAG=1,BRN_CRASHCAM_DIAG=1'
 Checks=@(
  @{Kind='NewAsserts';Name='no new assertions'}
  @{Kind='LogCount';Name='no stale vertex formats';Pattern='\[world-vd\] STALE';Max=0}
  @{Kind='LogCount';Name='no exceptions';Pattern='\[EXCEPTION\]';Max=0}
  @{Kind='LogCount';Name='all area transitions exercised';Pattern='\[sweep\] shot [0-4]/5';Min=5}
  @{Kind='LogCount';Name='declarations retire with streamed resources';Pattern='\[world-vd\] retired=';Min=1}
  @{Kind='LogMatch';Name='crash camera transition exercised';Pattern='\[crashcam\] container current state -> 2\b';Expect=$true}
  @{Kind='LogCount';Name='returns to driving camera';Pattern='\[crashcam\] container current state -> 1\b';Min=2}
 )
}
