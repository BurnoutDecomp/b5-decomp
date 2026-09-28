# L4 WORLDVFX (owner's list 2026-09-27) -- "Not glass breaking particle". A smashed window's Glass_shattering sparkles
# must draw ON the pane, live.
#
# THE DEFECT. cParticleEmitter::Emit @0x82914D38 hands InitialiseParticle the emitter's RAW velocity (var_90: the
# locator's mVel, `lvx128` of locator+0x40 at 0x82914E28 -> `stvx128` 0x82914E58; the sub-emitter arm's mParentVel,
# this+0x50, 0x82914DA4 -> 0x82914DC0) and advances only the spawn matrix's translation along it. InitialiseParticle
# @0x829116A8 scales that vector by mEmitterVelWeight into mLocatorVel (0x82911AF0..0x82911B10). The PC handed on the
# ADVANCED SPAWN POINT as the velocity, so every particle that inherits emitter velocity flew off at its own world
# position in m/s (~3.7 km/s in Paradise City): the Glass_shattering sprites drew 50-110 m from the pane on the frame
# after the shatter and were never seen. Unit test: tests/run_fxlion_emit_velocity.py.
#
# THE DRIVE is FxGlassNanLive's wall shot (crash_sweep 225 deg into the glass wall at (3249.8, -3.7, -1925.4)), with
# two faster shots after it so the car also crashes: the player's panes smash and fire Glass_shattering (GLINTERGLASS
# + TWINKY sprites).
# The witnesses (NOT IN THE X360 BINARY): [glassfx] (BRN_GLASS_DIAG=1: where FireGlassEffect seats the shatter and
# where UpdateVehicleEffectPositions re-seats it), [lionspawn] (the run's first LION particle: its spawn point and the
# velocity it inherits -- always on, one line) and [lionquad] (the first quad drawn per shape/texture, with its world
# centre -- always on, capped). NO FRAME DUMP (the frame pair is scratch/OWNERLIST_0927/evidence/L4).
#
# EXPECTED ON THE PRE-FIX EXE: FAIL -- the one-shot [lionspawn] prints vel == the spawn point (|vel| ~ 3764), the first
# GLINTERGLASS quad drew at (3216.05,-3.33,-2030.23) and the first TWINKY near (3225,-2037) while the shatter sat at
# (3171.6,-3.1,-2002.6) (l4_glass_film_wall/20260927_220144). ON THE FIXED EXE: every check passes.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxLionEmitVelocityLive.ps1

# The first [lionquad] of a texture after the first Glass_shattering start, and its distance from the nearest re-seated
# shatter effect printed before it. A script block (captured by each check's closure: run_case evaluates this file in a
# child scope).
$NearestShatter = {
  param($lines, $texture)
  $inv = [Globalization.CultureInfo]::InvariantCulture
  $start = -1
  for ($n = 0; $n -lt $lines.Count; $n++) { if ($lines[$n] -match '^\[lionstart\] #\d+ STARTED \S*Glass_shattering\.lef') { $start = $n; break } }
  if ($start -lt 0) { return @{ Pass = $false; Detail = 'no Glass_shattering start' } }
  $quad = -1
  for ($n = $start; $n -lt $lines.Count; $n++) { if ($lines[$n] -match ('^\[lionquad\]( BIG)? \w+(-\w+)? "' + $texture + '" .* centre=\((?<x>[^,]+),(?<y>[^,]+),(?<z>[^)]+)\)')) { $quad = $n; break } }
  if ($quad -lt 0) { return @{ Pass = $false; Detail = "no $texture quad drawn after the first Glass_shattering start" } }
  $qx = [double]::Parse($Matches.x, $inv); $qy = [double]::Parse($Matches.y, $inv); $qz = [double]::Parse($Matches.z, $inv)
  $best = [double]::MaxValue; $bestAt = ''
  for ($n = $start; $n -lt $quad; $n++) {
    if ($lines[$n] -match '^\[glassfx\] (fire|reseat) #\d+ .*? (effect\.)?wa=\((?<x>[^,]+),(?<y>[^,]+),(?<z>[^,)]+)') {
      $d = [math]::Sqrt([math]::Pow([double]::Parse($Matches.x, $inv) - $qx, 2) + [math]::Pow([double]::Parse($Matches.y, $inv) - $qy, 2) + [math]::Pow([double]::Parse($Matches.z, $inv) - $qz, 2))
      if ($d -lt $best) { $best = $d; $bestAt = $lines[$n].Substring(0, [math]::Min(40, $lines[$n].Length)) }
    }
  }
  if ($best -eq [double]::MaxValue) { return @{ Pass = $false; Detail = "no [glassfx] fire/reseat before the first $texture quad (BRN_GLASS_DIAG missing?)" } }
  return @{ Pass = ($best -le 5.0); Detail = ("first {0} quad at ({1:0.##},{2:0.##},{3:0.##}), {4:0.###} m from the nearest seated shatter ({5})" -f $texture, $qx, $qy, $qz, $best, $bestAt) }
}

@{
  Name    = 'fxlion_emit_velocity'
  Area    = 'vfx'
  Bug     = 'A smashed window''s Glass_shattering sparkles must draw on the pane: a LION particle inherits the emitter''s velocity, not its own spawn point (cParticleEmitter::Emit @0x82914D38).'
  Frames  = $false
  Run     = @{
    Drive            = $true
    MaxSeconds       = 150
    CrashSweep       = '3249.796,-3.7,-1925.404'
    CrashSweepShots  = '225:70,225:80,225:90'
    CrashSweepSettle = 240
    CrashSweepArm    = 4
  }
  DiagEnv = 'BRN_GLASS_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the sweep fired (the car was placed and launched)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'a smashed pane fired a Glass_shattering LION effect'
       Pattern = '^\[lionstart\] #\d+ STARTED \S*Glass_shattering\.lef'; Min = 1 }
    @{ Kind = 'Script';   Name = 'the run''s first LION particle inherits a VEHICLE-SIZED velocity (|vel| <= 150 m/s), not its spawn point (the pre-fix exe: vel == spawn.wa, |vel| ~ 3764)'; Script = {
        param($ctx)
        $inv = [Globalization.CultureInfo]::InvariantCulture
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[lionspawn\] spawn\.wa=\((?<sx>[^,]+),(?<sy>[^,]+),(?<sz>[^,]+),[^)]*\) .* vel=\((?<x>[^,]+),(?<y>[^,]+),(?<z>[^)]+)\)') {
            $v = [math]::Sqrt([math]::Pow([double]::Parse($Matches.x, $inv), 2) + [math]::Pow([double]::Parse($Matches.y, $inv), 2) + [math]::Pow([double]::Parse($Matches.z, $inv), 2))
            $same = ($Matches.x -eq $Matches.sx -and $Matches.y -eq $Matches.sy -and $Matches.z -eq $Matches.sz)
            return @{ Pass = ($v -le 150.0 -and -not $same); Detail = ("|vel| = {0:0.###} m/s{1}" -f $v, $(if ($same) { ' -- vel IS the spawn point' } else { '' })) }
          }
        }
        return @{ Pass = $false; Detail = 'no [lionspawn] line' }
      } }
    @{ Kind = 'Script'; Name = 'the first GLINTERGLASS sparkle after the shatter draws ON the pane: within 5 m of a seated shatter effect'; Script = {
        param($ctx) return (& $NearestShatter $ctx.LogLines 'GLINTERGLASS')
      }.GetNewClosure() }
    @{ Kind = 'Script'; Name = 'the first TWINKY sparkle after the shatter draws ON the pane: within 5 m of a seated shatter effect'; Script = {
        param($ctx) return (& $NearestShatter $ctx.LogLines 'TWINKY')
      }.GetNewClosure() }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
