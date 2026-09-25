# FX-CRASHVFX (crash parity 2026-09-25) -- the particle time step is ONE UPDATE FRAME's sum of sim sub-steps, live.
#
# BrnGameModule::OnStartOfUpdateFrame @0x823A8BB0 clears ParticleModule::mRenderData.mfCurrentTimeStep at the start of
# every update frame (EffectsModule::StartOfFrame -> ParticleModule::StartOfFrame, one inlined store: `stfsx f0(0.0),
# r11, r9` with r9 = 0x88194C = gameModule + 0x878B40 + 0x8E0C), and ParticleModule::Update adds each sub-step to it.
# The PC dropped the clear, so the published field was the sim seconds since boot: the [spark] ladder's dt= column read
# 10.6 at the first frame and 64.18 at t ~ 64 s (scratch/bugtest/runs/fxcrashvfx_reduced_frame_rate/20260925_104133),
# the debris integrated 21.8 s steps, and the spark ring, the trails and the debris each took a first difference.
# With the clear the field is the frame's step and every reader takes it VERBATIM.
#
# The drive is the standard wall shot (225 deg at 70 m/s: sparks, glass, debris, a crash camera at 30 Hz), then the
# throttle stays held to past sim time 300 s. The witnesses (NOT IN THE X360 BINARY, all default off):
#   [tstep]      BRN_TSTEP_DIAG=1 -- the published step at the first publish and past sim time 10 / 60 / 300 s, the
#                count of 0.0 publishes (MotionBlurState::Update's zero-step "reconstruct" arm, 0x823F84E4..0x823F8504),
#                and the running sum of every published value (what the field read without the clear);
#   [spark]      BRN_SPARK_DIAG=1 -- dt= the published field, rdt= the step the spark ring advanced by;
#   [debris-sim] BRN_DEBRIS_DIAG=1 -- dt= the step the debris jobs integrate;
#   [trailseg]   BRN_TRAIL_CADENCE=1 -- dt= the trail system's step on each new tyre-mark emitter;
#   [postfx-mb]  (always on, capped) -- the motion blur's MotionBlurState::Update ran on the published record.
# NO FRAME DUMP (the 2026-09-25 disk rule).
$ParseNumbers = {
  param($lines, $prefix, $field)
  $inv = [Globalization.CultureInfo]::InvariantCulture
  $out = @()
  foreach ($line in $lines) {
    if (-not $line.StartsWith($prefix)) { continue }
    if ($line -match (' ' + $field + '=(-?[0-9.]+|nan|-nan|inf|-inf)')) {
      $v = 0.0
      if ([double]::TryParse($Matches[1], [Globalization.NumberStyles]::Float, $inv, [ref]$v)) { $out += $v } else { $out += [double]::NaN }
    }
  }
  return ,$out
}

@{
  Name    = 'fxcrashvfx_timestep'
  Area    = 'vfx'
  Bug     = 'The particle time step must be one update frame of sim time, cleared at the start of every update frame (BrnGameModule::OnStartOfUpdateFrame), not the seconds since boot.'
  Frames  = $false
  Run     = @{
    Drive           = $true
    MaxSeconds      = 340
    CrashSweep      = '3249.796,-3.7,-1925.404'
    CrashSweepShots = '225:70'
    CrashSweepArm   = 4
  }
  DiagEnv = 'BRN_TSTEP_DIAG=1,BRN_SPARK_DIAG=1,BRN_DEBRIS_DIAG=1,BRN_TRAIL_CADENCE=1,BRN_GLASS_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the sweep fired (the car was placed and launched)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the car crashed (a crash record opened)'; Pattern = '\[crash-exit\] OPENED crash record'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the [tstep] witness reached every mark: the first publish and sim time 10 / 60 / 300 s'
       Pattern = '^\[tstep\] mark=(0|10|60|300) '; Min = 4 }
    @{ Kind = 'Script';   Name = 'the published step is ONE FRAME of sim time at every mark (<= 0.2 s), while the sum of all published steps -- what the field read without the clear -- has grown past 100 s by the 300 s mark'; Script = {
        param($ctx)
        $inv = [Globalization.CultureInfo]::InvariantCulture
        $rows = @(); $ok = $true; $sum300 = -1.0
        foreach ($line in $ctx.LogLines) {
          if ($line -notmatch '^\[tstep\] mark=(?<m>\d+) t=(?<t>[0-9.]+) frame=\d+ published=(?<p>[-0-9.a-z]+) zeroPublishes=(?<z>\d+)/(?<n>\d+) blurArm=(?<arm>\w+) sumOfPublished=(?<s>[0-9.]+)') { continue }
          $p = 0.0; $parsed = [double]::TryParse($Matches.p, [Globalization.NumberStyles]::Float, $inv, [ref]$p)
          if (-not $parsed -or $p -lt 0 -or $p -gt 0.2) { $ok = $false }
          if ($Matches.m -eq '300') { $sum300 = [double]::Parse($Matches.s, $inv) }
          $rows += "mark $($Matches.m): t=$($Matches.t) published=$($Matches.p) zero=$($Matches.z)/$($Matches.n) arm=$($Matches.arm) sum=$($Matches.s)"
        }
        return @{ Pass = ($ok -and $rows.Count -ge 4 -and $sum300 -ge 100.0); Detail = ($rows -join ' | ') }
      }.GetNewClosure() }
    @{ Kind = 'Script';   Name = 'the spark ring advances by the published field VERBATIM, one frame at a time ([spark] rdt = dt, both <= 0.2 s after the first line)'; Script = {
        param($ctx)
        $dt = & $ParseNumbers $ctx.LogLines '[spark] ' 'dt'
        $rdt = & $ParseNumbers $ctx.LogLines '[spark] ' 'rdt'
        $n = [math]::Min($dt.Count, $rdt.Count); $bad = 0; $max = 0.0; $first = ''
        for ($k = 1; $k -lt $n; $k++) {
          if ([math]::Abs($dt[$k] - $rdt[$k]) -gt 0.0002 -or $dt[$k] -gt 0.2 -or $dt[$k] -lt 0) { $bad++; if (-not $first) { $first = "line $k dt=$($dt[$k]) rdt=$($rdt[$k])" } }
          if ($dt[$k] -gt $max) { $max = $dt[$k] }
        }
        return @{ Pass = ($n -gt 1 -and $bad -eq 0); Detail = "$n [spark] lines, largest dt after the first $max, $bad off; $first" }
      }.GetNewClosure() }
    @{ Kind = 'Script';   Name = 'the debris jobs integrate one frame of sim time ([debris-sim] dt <= 0.2 s on every line) and collide in the crash'; Script = {
        param($ctx)
        $dt = & $ParseNumbers $ctx.LogLines '[debris-sim] ' 'dt'
        $bad = @($dt | Where-Object { $_ -gt 0.2 -or $_ -lt 0 -or [double]::IsNaN($_) }).Count
        $collide = @($ctx.LogLines | Where-Object { $_ -match '^\[debris-sim\] .* crash=1 .* collide=[1-9]' }).Count
        $max = 0.0; foreach ($v in $dt) { if ($v -gt $max) { $max = $v } }
        return @{ Pass = ($dt.Count -gt 0 -and $bad -eq 0 -and $collide -gt 0); Detail = "$($dt.Count) lines, largest dt $max, $bad off, $collide colliding crash lines" }
      }.GetNewClosure() }
    @{ Kind = 'Script';   Name = 'every new tyre-mark emitter sees a per-frame trail step ([trailseg] dt <= 0.2 s)'; Script = {
        param($ctx)
        $dt = & $ParseNumbers $ctx.LogLines '[trailseg] ' 'dt'
        $bad = @($dt | Where-Object { $_ -gt 0.2 -or $_ -lt 0 -or [double]::IsNaN($_) }).Count
        $timeouts = @($ctx.LogLines | Where-Object { $_ -match '^\[trailseg\] .* why=[a-z,]*timeout' }).Count
        return @{ Pass = ($bad -eq 0); Detail = "$($dt.Count) NEWEMIT line(s), $bad off, $timeouts ended by the 1.5-frame timeout" }
      }.GetNewClosure() }
    @{ Kind = 'LogCount'; Name = 'the motion blur consumes the published record (MotionBlurState::Update ran)'
       Pattern = '^\[postfx-mb\] apply-call \d+: active=\d updated=1 '; Min = 1 }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
