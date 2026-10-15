# An ordinary launch idles into attract mode with no Accept input. The last
# backbuffer must contain visible video rather than a WAITING_FOR_AUDIO black screen.
@{
  Name = 'audio_attract_startup'
  Area = 'audio'
  Bug = 'attract video omits its sound-name hash and waits forever for audio readiness'
  Frames = $true
  Run = @{ MaxSeconds=85; AcceptGap=3600; FrameEvery=120 }
  Checks = @(
    @{ Kind='LogCount'; Name='no assertions'; Pattern='\[ASSERT'; Max=0 }
    @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
    @{ Kind='Script'; Name='attract requests a nonzero audio stream'; Script={
      param($ctx)
      $attract=$false; $sound=$null
      foreach ($line in $ctx.LogLines) {
        if ($line -match "QueueNextMovie: file 'VIDEOS\\attract\.vp6'") { $attract=$true }
        elseif ($attract -and $line -match 'video audio requested: menu stream 0x([0-9A-Fa-f]+)') {
          $sound=[Convert]::ToUInt32($Matches[1], 16)
          break
        }
      }
      @{ Pass=($null -ne $sound -and $sound -ne 0); Detail="attract reached=$attract, audio stream=$sound" }
    } }
    @{ Kind='Frame'; Name='idle attract output is visible'; At='last'; Stat='lum_mean'; Min=5 }
  )
}
