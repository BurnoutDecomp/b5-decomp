# Owner's vehicle-switch/drop camera regression (2026-09-28).
# ARTIST ArbStateCarSelect::Update @8226F5D0 selects +298/RightToLeft
# for mbIsLeft=true, +294/LeftToRight for false. Four PC branches reversed
# that choice. Real menu inputs switch PCPD Special -> Cavalry -> PCPD.
# The copied fixture starts in Motor City; its authored destination takes
# are 424848 (right) then 445004 (left). No car/camera debug override.
# The input helper dismisses the first-entry title-update popup before
# browsing. The extra OptionNext during loading is intentionally retained
# from the reproduced A/B sequence; it does not complete another swap.
#
# A/B: codex_junkyard_switch_probe/{20260928_145559,20260928_150138}.
# Before: neither falling car was in the horizontal camera view (0/143).
# After: both are (143/143). PNGs corroborate the actual visible drops.
#   tools/tests/run_case.ps1 -Case b5-decomp/tests/JunkyardSwitchCameraLive.ps1 -Slot 5
#   ... -NoRun -RunDir <the A/B directory>  (old fails direction+view)
$lInputScript = Join-Path $PSScriptRoot 'run_junkyard_switch_camera_input.ps1'
$lSetup = {
  param($ctx)
  if ($ctx.Slot -le 0) { throw 'JunkyardSwitchCameraLive requires a test slot greater than zero.' }
  Start-Process powershell -WindowStyle Hidden -PassThru -ArgumentList @(
    '-NoProfile','-ExecutionPolicy','Bypass','-File',"`"$lInputScript`"",
    '-GameLog',"`"$($ctx.GameLog)`"",'-Slot',"$($ctx.Slot)"
  ) -RedirectStandardOutput (Join-Path $ctx.RunDir 'input.txt') -RedirectStandardError (Join-Path $ctx.RunDir 'input.err')
}.GetNewClosure()

@{
  Name = 'junkyard_switch_camera'
  Area = 'camera'
  Bug = 'Switching junkyard cars must play the take aimed at the destination bay and show the replacement vehicle falling into place.'
  ProfileFixture = 'scratch/OWNERLIST_0927/L4/profile/Profile.owner_converted.sav'
  Setup = $lSetup
  Frames = $true
  Run = @{ SkipIntro=$true; AcceptGap=1.0; HoldCarSelect=$true; MaxSeconds=65; FrameEvery=20 }
  DiagEnv = 'BRN_CRASHCAM_DIAG=1,BRN_DIRECTOR_TRACE=1,BRN_ICE_TRACE=1,BRN_CAMCOLLIDE_DIAG=1,BRN_CAMERA_TRACE=1,BRN_FRAME_DUMP_MAX=500'
  Checks = @(
    @{ Kind='Mark'; Name='reached car selection'; Cue='carsel' }
    @{ Kind='Script'; Name='real vehicle round trip completed'; Script={
      param($ctx)
      $models = @()
      foreach ($line in $ctx.LogLines) {
        if ($line -match 'STRM: Adding racecar for streaming: car=0, model=([^ ,]+)') {
          $model = $Matches[1]
          if ($models.Count -eq 0 -or $models[-1] -ne $model) { $models += $model }
        }
      }
      @{ Pass=(($models -join ',') -eq 'VEH_PUSCPIG,VEH_PUSMC0G,VEH_PUSCPIG'); Detail=($models -join ' -> ') }
    } }
    @{ Kind='LogCount'; Name='two camera drop sequences'; Pattern='\[crashcam\] carselect meState -> 10\b'; Min=2; Max=2 }
    @{ Kind='Script'; Name='both destination camera takes match ARTIST'; Script={
      param($ctx)
      $pending = $false; $takes = @()
      foreach ($line in $ctx.LogLines) {
        if ($line -match 'CarSelectManager: RequestChangeCar,') { $pending = $true }
        if ($pending -and $line -match '\[ice-prepare\] guid (\d+) takeData 1') {
          $takes += [int]$Matches[1]; $pending = $false
        }
      }
      @{ Pass=(($takes -join ',') -eq '424848,445004'); Detail=('destination take IDs: ' + ($takes -join ', ')) }
    } }
    @{ Kind='Script'; Name='both falling cars stay inside the horizontal camera view'; Script={
      param($ctx)
      $inv = [Globalization.CultureInfo]::InvariantCulture
      $drops = [Collections.Generic.List[object]]::new()
      $pending = $false; $drop = $null; $cam = $null
      foreach ($line in $ctx.LogLines) {
        if ($line -match 'CarSelectManager: RequestChangeCar,') { $pending = $true }
        if ($pending -and $line -match 'STRM: Adding racecar for streaming: car=0,') {
          $drop = @{ Samples=0; Visible=0; Low=[double]::MaxValue; High=[double]::MinValue }
          $drops.Add($drop); $pending = $false
        }
        if ($line -match '\[crashcam\] carselect meState -> 9\b') { $drop = $null }
        if ($line -match '^\[cam\] f=\d+ pos=([^ ]+) fwd=([^ ]+) fov=([^ ]+)') {
          $cam = @{
            Eye=@($Matches[1].Split(',') | ForEach-Object { [double]::Parse($_,$inv) })
            Forward=@($Matches[2].Split(',') | ForEach-Object { [double]::Parse($_,$inv) })
            Fov=[double]::Parse($Matches[3],$inv)
          }
        }
        if ($drop -and $cam -and $line -match '^\[camcol\].* car=([^ ]+) ') {
          $car = @($Matches[1].Split(',') | ForEach-Object { [double]::Parse($_,$inv) })
          $dx=$car[0]-$cam.Eye[0]; $dz=$car[2]-$cam.Eye[2]
          $length=[Math]::Sqrt(($dx*$dx+$dz*$dz)*($cam.Forward[0]*$cam.Forward[0]+$cam.Forward[2]*$cam.Forward[2]))
          $drop.Samples++
          if ($length -gt 0) {
            $cosine=($dx*$cam.Forward[0]+$dz*$cam.Forward[2])/$length
            if ($cosine -ge [Math]::Cos($cam.Fov*[Math]::PI/360.0)) { $drop.Visible++ }
          }
          $drop.Low=[Math]::Min($drop.Low,$car[1]); $drop.High=[Math]::Max($drop.High,$car[1])
        }
      }
      # Require the drop to have occurred, not merely a stationary car in view.
      # This is a measurement threshold only; no production tuning is changed.
      $pass=$drops.Count -eq 2
      $detail=@()
      foreach ($sample in $drops) {
        $fall=$sample.High-$sample.Low
        $pass=$pass -and $sample.Samples -ge 30 -and $sample.Visible -eq $sample.Samples -and $fall -gt 3.0
        $detail += ('{0}/{1} samples in view; height range {2:F2} m' -f $sample.Visible,$sample.Samples,$fall)
      }
      @{ Pass=$pass; Detail=($detail -join '; ') }
    } }
    @{ Kind='Mark'; Name='returned to driving'; Phase='DRIVING' }
    @{ Kind='NewAsserts'; Name='no new assertion families' }
    @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
  )
}
