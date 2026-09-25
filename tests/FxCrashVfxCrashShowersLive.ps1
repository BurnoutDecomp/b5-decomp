# FX-CRASHVFX item 1 (crash parity 2026-09-24/25) -- the race-car contact drain and its spark showers, live.
#
# ARTIST EffectsModule::ProcessRaceCarContacts @0x82297C08 drains the race-car contact queue every frame. A
# CRASHING car against the world sheds the crash impact dust (ParticleModule::SpawnSimple type 2, 2.5 particles
# per metre above 5 m/s), a crashing debris burst (FireDebrisBurst, record 5) and -- when it slides along the
# surface faster than 15 mph on a surface whose visualfxsurface allows sparks -- the crash spark shower
# (SpawnSparkShowerFromPoint, record 2, 100..250 sparks); a car GRINDING the world without crashing throws
# BurstAccumulator-paced world-grinding showers. Step 1 (b5 d5def70d) made the drain run: before it the drain was
# an announcing stub (no dust, no debris, no shower, one `[effects] NOT RECONSTRUCTED:
# EffectsModule::ProcessRaceCarContacts` line). Step 2 bodied the shower's consumer,
# ParticleModule::HandleSpawnSparkShowerFromPointEvent @0x82299CC8: before it every shower record was dropped
# with one `[particle] NOT RECONSTRUCTED: ParticleModule::HandleSpawnSparkShowerFromPointEvent` line, and the
# only sparks drawn were the along-line ones from EffectsModule::HandleSparkContacts -- a function the console
# never runs (byte_82CDB40C, its lbDisableThisEffect, is initialised TRUE), switched off in the same step.
#
# The drive is the standard wall shot (tools\diagnostics\crash_sweep_batch.ps1 -Headings 225 -Speeds 70
# -MinDamageableSeconds 1.6): target (3170.6, -3.7, -2004.6), launched 112 m out on heading 225 at 70 m/s, so the
# car is out of the post-placement no-damage window when it hits.
#
# The witnesses (NOT IN THE X360 BINARY, capped / change-gated):
#   BRN_SIMPLEFX_DIAG=1
#   [racecar-contact] crash dust car=<i> n=<n> speed=.. carry=.. t=.. at (x,y,z)        dust spawned this contact
#   [racecar-contact] crash shower car=<i> owner=<o> sparks=<n> size=.. vt=.. t=..       a crash shower posted
#   [racecar-contact] world-grinding shower car=<i> sparks=<n> size=.. speed=.. t=..     a grinding shower posted
#   BRN_SPARK_DIAG=1
#   [spark] prod{... line=<n> shower=<n>} ... live=<n> batches=<b> verts=<v> ... drew=<b>/<v> ...
#       shower = sparks the shower consumer handed SparkArray::SpawnSpark, line = along-line sparks (must stay 0),
#       live = sparks held by the banks now, verts = spark vertices built this frame, drew = cumulative drawn.
#
# NO FRAME DUMP in this case (the 2026-09-25 disk rule: 119 GB of dumps under scratch\bugtest\runs, D: at
# 16-18 GB). The picture witness is a separate, opt-in variant --
# scratch\CRASHPARITY_0922\fxcrashvfx_showers_frames.ps1 (every 12th present, flow_run's -MinFreeGB 25 guard)
# -- run only when a picture is the witness and the disk has the room.
@{
  Name    = 'fxcrashvfx_crash_showers'
  Area    = 'vfx'
  Bug     = 'A crashing car against a wall must shed the console''s crash impact dust and throw its crash / grinding spark showers: EffectsModule::ProcessRaceCarContacts has to run and ParticleModule::HandleSpawnSparkShowerFromPointEvent has to spawn the sparks, not announce itself; the along-line sparks the console never runs must stay off.'
  Frames  = $false
  Run     = @{
    Drive           = $true
    MaxSeconds      = 75
    CrashSweep      = '3249.796,-3.7,-1925.404'
    CrashSweepShots = '225:70'
    CrashSweepArm   = 4
  }
  DiagEnv = 'BRN_CRASH_RESPONSE_DIAG=1,BRN_SIMPLEFX_DIAG=1,BRN_SPARK_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the sweep fired (the car was placed and launched)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'Script';   Name = 'the launch is heading 225 (vx ~ vz ~ -49.5 m/s at the first post-shot tick above 60 mph)'; Script = {
        param($ctx)
        $shot = -1
        for ($i = 0; $i -lt $ctx.LogLines.Count; $i++) { if ($ctx.LogLines[$i] -match '^\[sweep\] shot 0/') { $shot = $i; break } }
        if ($shot -lt 0) { return @{ Pass = $false; Detail = 'no [sweep] shot line' } }
        $inv = [Globalization.CultureInfo]::InvariantCulture
        for ($i = $shot; $i -lt $ctx.LogLines.Count; $i++) {
          if ($ctx.LogLines[$i] -match 'pose tick f=\d+ mph=(?<m>[-\d.]+) .* v=\((?<x>[-\d.]+),(?<y>[-\d.]+),(?<z>[-\d.]+)\)' -and [double]::Parse($Matches.m, $inv) -gt 60.0) {
            $vx = [double]::Parse($Matches.x, $inv); $vz = [double]::Parse($Matches.z, $inv)
            $ok = ($vx -lt -40.0 -and $vz -lt -40.0)
            return @{ Pass = $ok; Detail = [string]::Format($inv, 'first fast tick v=({0:F1}, {1}, {2:F1}) -- heading 225 is (-49.5, ~0, -49.5)', $vx, $Matches.y, $vz) }
          }
        }
        return @{ Pass = $false; Detail = 'no post-shot pose tick above 60 mph' }
      } }
    @{ Kind = 'LogCount'; Name = 'the drain runs: no NOT RECONSTRUCTED announcement from ProcessRaceCarContacts'
       Pattern = 'NOT RECONSTRUCTED: EffectsModule::ProcessRaceCarContacts'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'the shower consumer runs: no NOT RECONSTRUCTED announcement from HandleSpawnSparkShowerFromPointEvent'
       Pattern = 'NOT RECONSTRUCTED: ParticleModule::HandleSpawnSparkShowerFromPointEvent'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'the player car''s wall crash shed CRASH IMPACT DUST (SpawnSimple type 2)'
       Pattern = '^\[racecar-contact\] crash dust car=0 n=[1-9]'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the drain posted at least one spark shower (crash or world-grinding)'
       Pattern = '^\[racecar-contact\] (crash|world-grinding) shower car=0 '; Min = 1 }
    @{ Kind = 'Script';   Name = 'the showers SPAWN sparks and the spark renderer DRAWS them ([spark] shower= > 0 with live, built and drawn vertices)'; Script = {
        param($ctx)
        $hits = 0; $first = $null; $maxShower = 0; $maxLive = 0
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[spark\] prod\{.*\sshower=(?<s>\d+)\}.*\slive=(?<live>\d+) batches=(?<b>-?\d+) verts=(?<v>\d+) .*\sdrew=(?<db>\d+)/(?<dv>\d+)') {
            $s = [int64]$Matches.s; $live = [int64]$Matches.live
            if ($s -gt $maxShower) { $maxShower = $s }
            if ($live -gt $maxLive) { $maxLive = $live }
            if ($s -gt 0 -and $live -gt 0 -and [int64]$Matches.v -gt 0 -and [int64]$Matches.dv -gt 0) {
              $hits++
              if (-not $first) { $first = ('shower={0} live={1} batches={2} verts={3} drew={4}/{5}' -f $Matches.s, $Matches.live, $Matches.b, $Matches.v, $Matches.db, $Matches.dv) }
            }
          }
        }
        return @{ Pass = ($hits -gt 0); Detail = "$hits ladder lines with shower sparks live, built and drawn (max shower=$maxShower max live=$maxLive); first: $first" }
      } }
    @{ Kind = 'LogCount'; Name = 'no along-line spark is spawned (HandleSparkContacts is console-dead: byte_82CDB40C == 1)'
       Pattern = '^\[spark\] prod\{.*\sline=[1-9]'; Max = 0 }
    @{ Kind = 'Script';   Name = 'census: crash showers and world-grinding showers the drain posted'; Script = {
        param($ctx)
        $crash = @($ctx.LogLines | Where-Object { $_ -match '^\[racecar-contact\] crash shower ' }).Count
        $grind = @($ctx.LogLines | Where-Object { $_ -match '^\[racecar-contact\] world-grinding shower ' }).Count
        $dust  = @($ctx.LogLines | Where-Object { $_ -match '^\[racecar-contact\] crash dust ' }).Count
        return @{ Pass = $true; Detail = "crash showers=$crash world-grinding showers=$grind crash-dust contacts=$dust" }
      } }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
