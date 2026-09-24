# FX-CRASHSND item 1 (crash parity 2026-09-24) -- the reset-on-track sting FLOOD.
#
# CameraControl::UpdateParams @0x826F6540 posts FxMessage_ResetOnTrack (type 5) when the published
# camera's E_FLAG_RACING_GAMEPLAY_CAMERA CHANGED and is SET and IS_PICTURE_PARADISE did not change
# (0x826F69F0..0x826F6ABC). The edge is only meaningful because MainDirector::Update @0x82274070
# rolls the published camera's PREVIOUS flag set to last frame's published current set
# (0x82275088 ldx this+0x33050 / 0x8227508C std lCamera.mState.mPreviousFlags) -- which the PC
# Update did not do. Measured before the fix: ~26 audible type-5 stings per placement, in bursts of
# 4 on consecutive frames (scratch/bugtest/runs/fxwatersnd_live/20260924_093629: 282 type-5 PLAYs in
# 3 shots), holding all four FxEffect voices and silently dropping other FX stings (the crash-in-water
# one in 20260924_091400).
#
# The witness is the POST, not the play: [fx-sting-post] (BRN_HUD_SOUND_DIAG) prints one line per
# reset-on-track post with the CameraControl update count. A post can only follow a real gameplay-
# camera ENTRY, so: never on two consecutive updates, and at most one per placement window (a
# window = from one placement -- a -CrashSweep seat or a reset-on-track RESULT -- to the next).
# Shots: the harbour rock-slope seat that drives into the water (a water wreck -> a reset), a 44 m/s
# run into the quay rocks (a crash -> a reset), then the slope seat again: its water entry lands
# right after a placement, where the pre-fix flood held every voice.
@{
  Name    = 'fxcrashsnd_reset_sting'
  Area    = 'sound'
  Bug     = 'The reset-on-track sting must fire once per return to the gameplay camera, not on every gameplay-camera frame (which flooded the four FX voices and dropped other stings).'
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
  DiagEnv = 'BRN_HUD_SOUND_DIAG=1,BRN_WRECK_LATCH_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'the sweep fired (the car was placed)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the reset-on-track post was dispatched (witness armed)'
       Pattern = '^\[fx-sting-post\] reset-on-track #\d+ update=\d+'; Min = 1 }
    @{ Kind = 'Script';     Name = 'no reset-on-track post on two consecutive CameraControl updates (the flood signature)'; Script = {
        param($ctx)
        $updates = @()
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[fx-sting-post\] reset-on-track #\d+ update=(?<u>\d+)') { $updates += [int64]$Matches.u }
        }
        if ($updates.Count -eq 0) { return @{ Pass = $false; Detail = 'no [fx-sting-post] line -- the witness never fired' } }
        $minGap = [int64]::MaxValue; $consecutive = 0
        for ($i = 1; $i -lt $updates.Count; $i++) {
          $gap = $updates[$i] - $updates[$i - 1]
          if ($gap -lt $minGap) { $minGap = $gap }
          if ($gap -le 1) { $consecutive++ }
        }
        $gapText = if ($updates.Count -gt 1) { "min gap $minGap updates" } else { 'single post' }
        return @{ Pass = ($consecutive -eq 0); Detail = ('{0} posts at updates [{1}]; {2}; consecutive pairs {3}' -f $updates.Count, ($updates -join ','), $gapText, $consecutive) }
      } }
    @{ Kind = 'Script';     Name = 'at most one reset-on-track post per placement window (seat or reset result)'; Script = {
        param($ctx)
        $windows = New-Object System.Collections.Generic.List[object]
        $current = [pscustomobject]@{ Label = 'boot'; Posts = 0 }
        $windows.Add($current)
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[sweep\] shot (?<k>\d+)/') {
            $current = [pscustomobject]@{ Label = ('seat ' + $Matches.k); Posts = 0 }; $windows.Add($current); continue }
          if ($l -match '^\[resetpump\] RESULT applied: global car 0 ') {
            $current = [pscustomobject]@{ Label = 'reset'; Posts = 0 }; $windows.Add($current); continue }
          if ($l -match '^\[fx-sting-post\] reset-on-track ') { $current.Posts++ }
        }
        $total = ($windows | Measure-Object -Property Posts -Sum).Sum
        $worst = ($windows | Measure-Object -Property Posts -Maximum).Maximum
        $text = ($windows | ForEach-Object { '{0}:{1}' -f $_.Label, $_.Posts }) -join ' '
        return @{ Pass = ($worst -le 1 -and $total -ge 1); Detail = ('{0} posts over {1} windows (max {2} per window) -- {3}' -f $total, $windows.Count, $worst, $text) }
      } }
    @{ Kind = 'Script';     Name = 'at most one reset-on-track sting PLAYS per placement window (scores pre-fix logs too)'; Script = {
        param($ctx)
        $windows = New-Object System.Collections.Generic.List[object]
        $current = [pscustomobject]@{ Label = 'boot'; Plays = 0 }
        $windows.Add($current)
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[sweep\] shot (?<k>\d+)/') {
            $current = [pscustomobject]@{ Label = ('seat ' + $Matches.k); Plays = 0 }; $windows.Add($current); continue }
          if ($l -match '^\[resetpump\] RESULT applied: global car 0 ') {
            $current = [pscustomobject]@{ Label = 'reset'; Plays = 0 }; $windows.Add($current); continue }
          if ($l -match '^\[fx-sound-msg\] type=5 slot=\d+ -> PLAY ') { $current.Plays++ }
        }
        $total = ($windows | Measure-Object -Property Plays -Sum).Sum
        $worst = ($windows | Measure-Object -Property Plays -Maximum).Maximum
        $text = ($windows | ForEach-Object { '{0}:{1}' -f $_.Label, $_.Plays }) -join ' '
        return @{ Pass = ($worst -le 1 -and $total -ge 1); Detail = ('{0} type-5 plays over {1} windows (max {2} per window) -- {3}' -f $total, $windows.Count, $worst, $text) }
      } }
    @{ Kind = 'LogCount';   Name = 'the crash-in-water sting still reaches a voice (type 8 PLAY)'
       Pattern = '\[fx-sound-msg\] type=8 slot=\d+ -> PLAY '; Min = 1 }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
