# FX-CRASHVFX (crash parity 2026-09-25) -- the particle module enters its REDUCED-FRAME-RATE arm in a crash, live.
#
# BrnGameModule::DoDispatch @0x823DC458 (0x823DC5C8..0x823DC630) writes DispatchThreadInputBuffer::
# mbIsRenderingAtFullFrameRate every frame from the director camera's current flag set: full rate only for
# E_FLAG_RACING_GAMEPLAY_CAMERA (bit 3) or E_FLAG_ROAD_FOLLOWING_CAM (bit 27). ParticleModule::GenerateRenderRequests
# @0x82281BD8 turns a reduced rate into eRenderDataFlagReducedFrameRate (0x40) -- the crash-mode switch for debris
# collision, the 10 s bucket window, the full debris lifetimes and the crash CPU monitors. The PC never wrote the
# flag (DispatchThreadInputBuffer::Construct's `true` stood for ever), so the bit was never set: the pre-fix
# wall-crash run scratch\bugtest\runs\fxcrashvfx_showers_noframes\20260925_065746 shows only flags 0x003E / 0x00BE /
# 0x00BF on its 534 [spark] ladder lines, the crash included.
#
# The drive is the standard wall shot (crash_sweep_batch -Headings 225 -Speeds 70 -MinDamageableSeconds 1.6).
# The witnesses (NOT IN THE X360 BINARY): the [spark] ladder's flags= word (BRN_SPARK_DIAG=1) and the
# [debris-sim] mode edge lines (BRN_DEBRIS_DIAG=1). NO FRAME DUMP.
#
# The rate follows the CAMERA, not the crash record: the boot's junkyard / tour camera is not a racing-gameplay
# camera either, so the run starts REDUCED and turns FULL when the director hands the gameplay camera over. The
# not-stuck check is therefore a FULL edge after a REDUCED one, not "FULL before the shot" (first post-fix run
# 20260925_104133: REDUCED f=1 junkyard, FULL f=707, REDUCED f=2438 in the second crash, FULL f=2572 after
# its CRASH COMPLETE).
@{
  Name    = 'fxcrashvfx_reduced_frame_rate'
  Area    = 'vfx'
  Bug     = 'In a crash the particle module must run its reduced-frame-rate arm (render data 0x40): BrnGameModule::DoDispatch has to write mbIsRenderingAtFullFrameRate from the camera flags, full rate only for the racing-gameplay / road-following cameras.'
  Frames  = $false
  Run     = @{
    Drive           = $true
    MaxSeconds      = 75
    CrashSweep      = '3249.796,-3.7,-1925.404'
    CrashSweepShots = '225:70'
    CrashSweepArm   = 4
  }
  DiagEnv = 'BRN_CRASH_RESPONSE_DIAG=1,BRN_SPARK_DIAG=1,BRN_DEBRIS_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the sweep fired (the car was placed and launched)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the car crashed (a crash record opened)'; Pattern = '\[crash-exit\] OPENED crash record'; Min = 1 }
    @{ Kind = 'Script';   Name = 'the rate is not stuck: a FULL-FRAME-RATE edge follows a REDUCED one (gameplay camera back)'; Script = {
        param($ctx)
        $reduced = -1; $full = -1; $edges = @()
        for ($i = 0; $i -lt $ctx.LogLines.Count; $i++) {
          if ($ctx.LogLines[$i] -match '^\[debris-sim\] mode (?<m>REDUCED|FULL)-FRAME-RATE .* f=(?<f>\d+) t=(?<t>[0-9.]+)') {
            $edges += "$($Matches.m)@f$($Matches.f)"
            if ($Matches.m -eq 'REDUCED') { if ($reduced -lt 0) { $reduced = $i } }
            elseif ($reduced -ge 0 -and $full -lt 0) { $full = $i }
          }
        }
        return @{ Pass = ($reduced -ge 0 -and $full -gt $reduced); Detail = "mode edges: $($edges -join ', ')" }
      } }
    @{ Kind = 'Script';   Name = 'the crash renders at the REDUCED rate: a [spark] line after the crash record opened has 0x40 set'; Script = {
        param($ctx)
        $crash = -1
        for ($i = 0; $i -lt $ctx.LogLines.Count; $i++) { if ($ctx.LogLines[$i] -match '^\[crash-exit\] OPENED crash record') { $crash = $i; break } }
        if ($crash -lt 0) { return @{ Pass = $false; Detail = 'no crash record' } }
        $set = 0; $clear = 0; $first = ''
        for ($i = $crash; $i -lt $ctx.LogLines.Count; $i++) {
          if ($ctx.LogLines[$i] -match '^\[spark\] prod\{.* flags=0x(?<f>[0-9A-F]+) ') {
            if (([Convert]::ToInt32($Matches.f, 16) -band 0x40) -ne 0) { $set++; if (-not $first) { $first = "flags=0x$($Matches.f)" } } else { $clear++ }
          }
        }
        return @{ Pass = ($set -gt 0); Detail = "after the crash opened: $set ladder lines with 0x40 set (first $first), $clear clear" }
      } }
    @{ Kind = 'LogCount'; Name = 'the debris simulation saw the switch (mode edge line)'
       Pattern = '^\[debris-sim\] mode REDUCED-FRAME-RATE'; Min = 1 }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
