# FX-GLASSNAN (crash parity wave 5, 2026-09-26) -- a smashed window's 'Glass_shattering' effect spawns its particles at
# FINITE points on the pane, live.
#
# THE DEFECT. BrnEffects::BrnEffectsGlassManager::FireGlassEffect @0x82295D50 caches every shatter in its car's frame,
# mLocalTransform = lEffectTransform * Inverse(lVehicleTransform), and ::UpdateVehicleEffectPositions @0x8228D208 re-seats
# the LION effect every frame as mLocalTransform * the car's transform. The console INLINES the affine
# rw::math::vpu::Inverse and operator* (x, y, z lanes only). The PC called the general 4x4 Inverse @0x825B2628; a
# Matrix44Affine's w column is (0,0,0,0) (the console's own SetIdentity, inlined in Reset @0x8228F298, writes it, and so
# do the PC producers), so the 4x4 determinant was 0, mLocalTransform was 0 * inf = NaN, and every re-seat -- the locator
# every glass particle spawns from -- was NaN. Unit test: tests/run_fxglassnan.py (the console's words on emu64).
#
# THE DRIVE is FxCrashVfxGlassLive's wall shot (crash_sweep -Headings 225 -Speeds 70, bit-deterministic): the player's
# race car smashes four panes in one frame and fires five Glass_shattering effects, and they are the FIRST LION effects of
# the run, so the one-shot [lionspawn] witness (cParticleEmitter::Emit, the first LION particle) is a glass particle.
# The witnesses (NOT IN THE X360 BINARY, BRN_GLASS_DIAG=1): [glass] (one line per pane the drain reads) and, new with this
# fix, [glassfx] -- the first 16 seated fires (the shatter's world point, the slot's mLocalTransform translation, the car's
# w column) and the first 48 re-seats (the LION effect's new translation and its distance from its car). NO FRAME DUMP.
#
# ⛔ EXPECTED ON THE PRE-FIX EXE (7fdddf649d45, b5 08be7e89, the final live verification's): FAIL. It has no [glassfx]
# witness, so every [glassfx] check finds nothing, and its one-shot [lionspawn] is
#     [lionspawn] spawn.wa=(-nan(ind),-nan(ind),-nan(ind),1.00) locator.wa=(-nan(ind),...) vel=(-nan(ind),...)
# as in every banked run of this recipe (fxcrashvfx_glass/20260925_123012, _141221, _143927, _152842) and in the final
# verification's fxscenarios_traffic_check_probe/20260926_002333 (line 7084). ON THE FIXED EXE: every check passes.

# The [glassfx] reseat lines, parsed. A script block, captured by each check's closure, because run_case evaluates this
# file in a child scope (a function would be gone by check time).
$GetReseats = {
  param($lines)
  $inv = [Globalization.CultureInfo]::InvariantCulture
  $out = [System.Collections.ArrayList]::new()
  for ($n = 0; $n -lt $lines.Count; $n++) {
    if ($lines[$n] -notmatch '^\[glassfx\] reseat #\d+ slot=(?<slot>\d+) owner=0x[0-9A-F]+ \w+ wa=\((?<x>[^,]+),(?<y>[^,]+),(?<z>.+?)\) car\.wa=\([^)]*\) dist=(?<d>\S+) finite=(?<f>\d)') { continue }
    $x = 0.0; $y = 0.0; $z = 0.0; $d = 0.0
    $ok = [double]::TryParse($Matches.x, [Globalization.NumberStyles]::Float, $inv, [ref]$x) -and
          [double]::TryParse($Matches.y, [Globalization.NumberStyles]::Float, $inv, [ref]$y) -and
          [double]::TryParse($Matches.z, [Globalization.NumberStyles]::Float, $inv, [ref]$z) -and
          [double]::TryParse($Matches.d, [Globalization.NumberStyles]::Float, $inv, [ref]$d)
    [void]$out.Add([pscustomobject]@{ Line = $n; Slot = [int]$Matches.slot; X = $x; Y = $y; Z = $z; Dist = $d
                                      Finite = ($ok -and [int]$Matches.f -eq 1) })
  }
  return ,$out
}

@{
  Name    = 'fxglassnan'
  Area    = 'vfx'
  Bug     = 'A smashed window must show its Glass_shattering effect: the shatter''s particles spawn at finite points on the pane (they spawned at NaN: FireGlassEffect divided by the zero determinant of the car''s 4x4).'
  Frames  = $false
  Run     = @{
    Drive           = $true
    MaxSeconds      = 75
    CrashSweep      = '3249.796,-3.7,-1925.404'
    CrashSweepShots = '225:70'
    CrashSweepArm   = 4
  }
  DiagEnv = 'BRN_GLASS_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the sweep fired (the car was placed and launched)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the car crashed (a crash record opened)'; Pattern = '\[crash-exit\] OPENED crash record'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'a smashed pane was handled and asked for at least one shatter ([glass] handled=1 shatters>=1)'
       Pattern = '^\[glass\] event .* handled=1 emit=\d shatters=[1-9]'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'a Glass_shattering LION effect started'
       Pattern = '^\[lionstart\] #\d+ STARTED \S*Glass_shattering\.lef'; Min = 1 }
    # ---- FireGlassEffect: the slot's cached transform --------------------------------------------------------------
    @{ Kind = 'LogCount'; Name = 'FireGlassEffect seated a shatter slot with a FINITE mLocalTransform ([glassfx] fire local.finite=1)'
       Pattern = '^\[glassfx\] fire #\d+ .* local\.finite=1$'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'no seated shatter slot has a NaN / inf mLocalTransform ([glassfx] fire local.finite=0)'
       Pattern = '^\[glassfx\] fire #\d+ .* local\.finite=0$'; Max = 0 }
    @{ Kind = 'LogValue'; Name = 'every cached shatter lies ON its car: |mLocalTransform.wAxis| <= 4 m (a pane is at most ~2.7 m from the car''s origin)'
       Pattern = '^\[glassfx\] fire #\d+ .* \|local\.wa\|=(?<d>\S+) '; Group = 'd'; Agg = 'all'; Min = 0; Max = 4 }
    @{ Kind = 'LogCount'; Name = 'root cause, live: the car transform FireGlassEffect is handed has the w column (0,0,0,0) the 4x4 inverse divided by zero on'
       Pattern = '^\[glassfx\] fire #\d+ .* car\.w=\(-?0,-?0,-?0,-?0\) '; Min = 1 }
    # ---- UpdateVehicleEffectPositions: the LION effect's transform, the locator the particles spawn from -------------
    @{ Kind = 'LogCount'; Name = 'no re-seated shatter effect is NaN / inf ([glassfx] reseat finite=0)'
       Pattern = '^\[glassfx\] reseat #\d+ .* finite=0$'; Max = 0 }
    @{ Kind = 'Script';   Name = 'every re-seated shatter effect is FINITE and ON its car (within 4 m of the car''s origin)'; Script = {
        param($ctx)
        $rs = & $GetReseats $ctx.LogLines
        if ($rs.Count -eq 0) { return @{ Pass = $false; Detail = 'no [glassfx] reseat line -- the witness never fired (the pre-fix exe has none)' } }
        $bad = @($rs | Where-Object { -not $_.Finite -or $_.Dist -gt 4.0 })
        $first = $rs[0]
        return @{ Pass = ($bad.Count -eq 0); Detail = ("{0} re-seat(s), {1} NaN or off the car; first: slot {2} at ({3:0.###},{4:0.###},{5:0.###}) {6:0.###} m from its car" -f $rs.Count, $bad.Count, $first.Slot, $first.X, $first.Y, $first.Z, $first.Dist) }
      }.GetNewClosure() }
    # ---- the particles: the run's first LION particle is a glass shatter one in this recipe --------------------------
    @{ Kind = 'LogCount'; Name = 'the first LION particle spawned ([lionspawn], one-shot)'; Pattern = '^\[lionspawn\] spawn\.wa='; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'it spawned at a FINITE point (no nan / inf in spawn.wa, locator.wa or vel) -- the pre-fix exe prints -nan(ind)'
       Pattern = '^\[lionspawn\] .*(nan|inf)'; Max = 0 }
    @{ Kind = 'Script';   Name = 'it is a Glass_shattering particle and it spawned ON the pane: within 5 m of a re-seated shatter effect'; Script = {
        param($ctx)
        $lines = $ctx.LogLines
        $spawn = -1
        for ($n = 0; $n -lt $lines.Count; $n++) { if ($lines[$n] -match '^\[lionspawn\] spawn\.wa=') { $spawn = $n; break } }
        if ($spawn -lt 0) { return @{ Pass = $false; Detail = 'no [lionspawn] line' } }
        $lastStart = ''
        for ($n = $spawn - 1; $n -ge 0; $n--) { if ($lines[$n] -match '^\[lionstart\] #\d+ STARTED (?<e>\S+)') { $lastStart = $Matches.e; break } }
        if ($lastStart -notmatch 'Glass_shattering\.lef') {
          return @{ Pass = $false; Detail = "the one-shot spawn follows '$lastStart', not a Glass_shattering start -- it cannot speak for the shatter (the recipe changed?)" }
        }
        $inv = [Globalization.CultureInfo]::InvariantCulture
        if ($lines[$spawn] -notmatch 'spawn\.wa=\((?<x>[^,]+),(?<y>[^,]+),(?<z>[^,]+),') { return @{ Pass = $false; Detail = 'unparsable [lionspawn] line' } }
        $sx = 0.0; $sy = 0.0; $sz = 0.0
        if (-not ([double]::TryParse($Matches.x, [Globalization.NumberStyles]::Float, $inv, [ref]$sx) -and
                  [double]::TryParse($Matches.y, [Globalization.NumberStyles]::Float, $inv, [ref]$sy) -and
                  [double]::TryParse($Matches.z, [Globalization.NumberStyles]::Float, $inv, [ref]$sz))) {
          return @{ Pass = $false; Detail = ("the glass particle spawned at a non-number: {0}" -f $lines[$spawn]) }
        }
        $all = & $GetReseats $lines
        $rs = @($all | Where-Object { $_.Line -lt $spawn -and $_.Finite })
        if ($rs.Count -eq 0) { return @{ Pass = $false; Detail = 'no finite [glassfx] re-seat before the spawn' } }
        $best = [double]::MaxValue
        foreach ($r in $rs) {
          $d = [math]::Sqrt(($r.X - $sx) * ($r.X - $sx) + ($r.Y - $sy) * ($r.Y - $sy) + ($r.Z - $sz) * ($r.Z - $sz))
          if ($d -lt $best) { $best = $d }
        }
        return @{ Pass = ($best -le 5.0); Detail = ("glass particle at ({0:0.##},{1:0.##},{2:0.##}); nearest re-seated shatter {3:0.###} m away ({4} re-seats before it)" -f $sx, $sy, $sz, $best, $rs.Count) }
      }.GetNewClosure() }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
