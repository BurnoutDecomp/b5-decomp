# FX-CRASHVFX (crash parity 2026-09-25, item 6b) -- a prop struck near the camera FIRES ITS MATERIAL'S LION EFFECTS, live.
#
# BrnEffects::PropCollisions is the prop-strike VFX ParticleModule embeds at +0x4270. LoadFXBundle @0x8229C950 binds
# the "vfx_props_collection" from particles.bundle (stage 14). It then asks the game-data module for the prop physics
# data (stage 17, the LoadGameDataEvent PropEntityModule posts for itself: pool 1, id 0xA773D7113DF454BF), binds that
# data's handle and runs Initialise, which builds the prop-type -> material table (stage 18).
# Every sim step, EffectsModule::Update @0x8229EC28 hands the prop VFX locator queue to UpdateLocatorVfx @0x822993A0.
# Each struck prop within 50 m of the camera fires, through TriggerLocators @0x82299168, a LION effect on each locator
# of its material: the smashing material for a SMASH or above 50 mph, else the unbroken one. Before item 6b none of
# this was wired: the three LoadFXBundle stages and the Update call announced themselves NOT RECONSTRUCTED.
#
# The drive is props_hit_speed's (tools/tests/cases/props_hit_speed.ps1): teleport onto the straight at
# (2995, 1.5, -1750) facing +Z, full throttle, and ~200 m later the car reaches the physical prop cluster around
# (2988..3002, 1.4, -1548.6) at 35-40 m/s, which is well above 50 mph.
# The witnesses (NOT IN THE X360 BINARY, default off, capped):
#   [prop-vfx]  BRN_PROP_VFX_DIAG=1 -- Initialise's table, one line per locator event (its type, the car's speed, the
#               material chosen, the verdict) and one per locator fired (its name / hash, the LION handle, whether the
#               effect's description resolved, where it was placed);
#   [Q6-world]  BRN_PROP_DIAG=1 (props_hit_speed's) -- the struck props' world positions, so a miss is visible.
# NO FRAME DUMP.
$GetEventRows = {
  param($lines)
  $inv = [Globalization.CultureInfo]::InvariantCulture
  $rows = @()
  foreach ($line in $lines) {
    if ($line -notmatch '^\[prop-vfx\] t=(?<t>\S+) event (?<i>\d+)/(?<n>\d+) type=(?<type>\d+) (?<kind>SMASH|HIT) speed=(?<speed>\S+) mph at \((?<pos>[^)]*)\) -> (?<material>smashing|unbroken) material: (?<verdict>[^(]+?) \((?<loc>\d+) locator\(s\)\)$') { continue }
    $speed = 0.0
    [void][double]::TryParse($Matches.speed, [Globalization.NumberStyles]::Float, $inv, [ref]$speed)
    $rows += [pscustomobject]@{ T = $Matches.t; Type = [int]$Matches.type; Kind = $Matches.kind; Speed = $speed
      SpeedText = $Matches.speed; Material = $Matches.material; Verdict = $Matches.verdict.Trim()
      Locators = [int]$Matches.loc; Line = $line }
  }
  return ,$rows
}

@{
  Name    = 'fxcrashvfx_prop_smash'
  Area    = 'vfx'
  Bug     = 'A prop struck near the camera must fire the LION effects on its material''s locators (the smashing material above 50 mph), placed along the car''s travel.'
  Frames  = $false
  Run     = @{
    Drive          = $true
    MotionProbe    = $true
    MaxSeconds     = 60
    SkipIntro      = $true      # the console -skipvideos latch (see props_hit_speed.ps1)
    AcceptGap      = 1.0        # harness pump latency, not a game gate
    Teleport       = '2995,1.5,-1750,0'    # props_hit_speed's shot: 200 m of run-up into the type-24 prop cluster
    ThrottleScript = '0:accel'
  }
  DiagEnv = 'BRN_PROP_VFX_DIAG=1,BRN_PROP_DIAG=1'
  Checks  = @(
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'the prop-strike VFX runs: no PropCollisions / LoadFXBundle 14 / 17 / 18 announcement'
       Pattern = 'NOT RECONSTRUCTED: (LoadFXBundle stage 14|LoadFXBundle stages 17/18|BrnEffects::PropCollisions)'; Max = 0 }
    @{ Kind = 'Script'; Name = 'the prop physics bundle is served to BOTH requests (the effects module''s and the prop module''s); the second is ref-counted, not reloaded'; Script = {
        param($ctx)
        $loads = @($ctx.LogLines | Where-Object { $_ -match "^\[stream\] LoadBundle 'Props/PropPhysics\.bundle' -> pool 1: (\d+) resources" })
        $shared = @($loads | Where-Object { $_ -match ': 0 resources$' })
        @{ Pass = ($loads.Count -ge 2 -and $shared.Count -ge 1); Detail = "$($loads.Count) load(s), $($shared.Count) served from the resident copy: $($loads -join ' | ')" }
      } }
    @{ Kind = 'Script'; Name = 'LoadFXBundle stage 18 completed: Initialise built the table from the bound collection and physics data, then the ladder finished'; Script = {
        param($ctx)
        $init = @($ctx.LogLines | Where-Object { $_ -match '^\[prop-vfx\] Initialise: collection (?<c>\S+) \((?<p>\d+) props, version (?<v>\d+)\), physics data (?<d>\S+); (?<u>\d+) prop types with an unbroken material, (?<s>\d+) with a smashing one' })
        if ($init.Count -lt 1) { return @{ Pass = $false; Detail = 'no [prop-vfx] Initialise line' } }
        [void]($init[0] -match '^\[prop-vfx\] Initialise: collection (?<c>\S+) \((?<p>\d+) props, version (?<v>\d+)\), physics data (?<d>\S+); (?<u>\d+) prop types with an unbroken material, (?<s>\d+) with a smashing one')
        $props = [int]$Matches.p; $version = [int]$Matches.v; $unbroken = [int]$Matches.u; $smashing = [int]$Matches.s
        $nullData = $Matches.d -match '^(0x)?0+$'
        $done = @($ctx.LogLines | Where-Object { $_ -match '^\[skid-ready\] LoadFXBundle DONE' }).Count
        @{ Pass = ($props -gt 0 -and $version -eq 3 -and -not $nullData -and ($unbroken + $smashing) -gt 0 -and $done -ge 1)
           Detail = "$($init[0]) ; LoadFXBundle DONE lines: $done" }
      } }
    @{ Kind = 'Script'; Name = 'every visible event takes the console''s material: smashing for a SMASH or above 50 mph (`fcmpu ; ble` 0x82299484), else unbroken'; Script = {
        param($ctx)
        $rows = & $GetEventRows $ctx.LogLines
        $bad = @()
        foreach ($r in $rows) {
          if ([math]::Abs($r.Speed - 50.0) -lt 0.06) { continue }   # printed to 0.1 mph: not judged at the edge
          $expect = if ($r.Kind -eq 'SMASH' -or $r.Speed -gt 50.0) { 'smashing' } else { 'unbroken' }
          if ($r.Material -ne $expect) { $bad += "$($r.Line) (expected $expect)" }
        }
        @{ Pass = ($rows.Count -gt 0 -and $bad.Count -eq 0); Detail = "$($rows.Count) visible event(s), $($bad.Count) off the gate $(($bad | Select-Object -First 2) -join ' | ')" }
      }.GetNewClosure() }
    @{ Kind = 'Script'; Name = 'a struck prop fired its material''s locators (UpdateLocatorVfx -> TriggerLocators)'; Script = {
        param($ctx)
        $rows = & $GetEventRows $ctx.LogLines
        $fired = @($rows | Where-Object { $_.Verdict -eq 'TriggerLocators' -and $_.Locators -gt 0 })
        $first = if ($fired.Count) { $fired[0].Line } else { '' }
        $verdicts = ($rows | Group-Object Verdict | ForEach-Object { "$($_.Name)=$($_.Count)" }) -join ', '
        @{ Pass = ($fired.Count -gt 0); Detail = "$($fired.Count) event(s) fired ($verdicts); first: $first" }
      }.GetNewClosure() }
    @{ Kind = 'Script'; Name = 'a locator''s LION effect started and its description resolved (the prop''s own effect, placed on the prop)'; Script = {
        param($ctx)
        $locs = @($ctx.LogLines | Where-Object { $_ -match '^\[prop-vfx\]   t=\S+ locator \d+/\d+ ' })
        $resolved = @($locs | Where-Object { $_ -match '-> LION handle=0x[0-9A-F]{8} description=resolved at \(' })
        $unresolved = @($locs | Where-Object { $_ -match 'no LION effect|description=null' })
        $first = if ($resolved.Count) { $resolved[0] } else { '' }
        @{ Pass = ($resolved.Count -gt 0); Detail = "$($locs.Count) locator line(s): $($resolved.Count) resolved, $($unresolved.Count) not; first: $first" }
      } }
  )
}
