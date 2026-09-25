# FX-CRASHVFX C3 (crash parity 2026-09-25, CC-15) -- THE DEBRIS MESHES LOAD AND DRAW, live.
#
# At boot ParticleModule::LoadFXBundle @0x8229C950 walks stages 10 -> 5 -> 6 -> 7 -> 8 -> 11: one mesh collection and
# one texture per debris array out of PARTICLES.BUNDLE (lowres_debris.rf3 x3 / WHITE, highres_debris_02.rf3 /
# highres_debris, Glass_debris.rf3 / glass_debris). BrnDebrisRenderer::RenderDebrisArray then instances each array's
# mesh 32 pieces per draw. Before C3 the ladder went 10 -> 11, the collections were neither registered nor ported, and
# every array was skipped at draw time ("[debrispass] array SKIPPED: mMeshCollection is null"). Even bound, the draw
# would have been dropped: MeshHelper::Dispatch binds stride 0 (the console's "use the shader's vfetch stride") and
# the PC fast-set path skipped a stride-0 stream.
#
# The drive is the glass case's (FxCrashVfxGlassLive): one launch at 70 m/s into the glass wall at
# (3249.8, -3.7, -1925.4) -- a crash (the crash-debris burst, arrays 0..3) and a smashed pane (glass debris, array 4).
# The witnesses (NOT IN THE X360 BINARY, default off, capped):
#   [fxbundle]    BRN_DEBRIS_DIAG=1 -- once, what each debris array holds after stage 8 (mesh, texture name, texture);
#   [debrispass]  BRN_DEBRIS_DIAG=1 -- per array draw: batches, instances, the mesh's index / vertex counts, and d3d=,
#                 the draws D3D ACCEPTED during them (renderengine::WorldDrawCallCount counts SUCCEEDED only),
#                 and us=, the CPU time of the array's pass;
#   [stride0]     BRN_STRIDE0_DIAG=1 -- who draws with a stride-0 fast-set stream, and the stride it is drawn at.
# NO FRAME DUMP.
@{
  Name    = 'fxcrashvfx_debris_draw'
  Area    = 'vfx'
  Bug     = 'Crash and glass debris must draw: LoadFXBundle has to bind the five debris arrays their mesh collections and textures (stages 5..8), the ported collections must fix up, and the debris draw must reach D3D.'
  Frames  = $false
  Run     = @{
    Drive           = $true
    MaxSeconds      = 75
    CrashSweep      = '3249.796,-3.7,-1925.404'
    CrashSweepShots = '225:70'
    CrashSweepArm   = 4
  }
  DiagEnv = 'BRN_DEBRIS_DIAG=1,BRN_STRIDE0_DIAG=1,BRN_GLASS_DIAG=1,BRN_DEBRIS_DIAG_ARRAY=4'
  Checks  = @(
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'the sweep fired (the car was placed and launched)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the car crashed (a crash record opened)'; Pattern = '\[crash-exit\] OPENED crash record'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the installed PARTICLES.BUNDLE carries the PORTED meshes (no refusal line)'
       Pattern = 'PARTICLES\.BUNDLE holds an UNCONVERTED'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'the FX-bundle ladder finished (no stall in stages 5..8)'; Pattern = '\[skid-ready\] LoadFXBundle DONE'; Min = 1 }
    @{ Kind = 'Script'; Name = 'stages 5..8 bound all five debris arrays a mesh and the texture it names'; Script = {
        param($ctx)
        $rx = '^\[fxbundle\] stages 5-8 done: debris array (?<a>\d) ''(?<m>[^'']+)'' mesh=(?<mp>[0-9A-Fa-f]+) texture ''(?<t>[^'']+)'' -> (?<tp>[0-9A-Fa-f]+)$'
        $bound = @(); $all = @()
        foreach ($l in $ctx.LogLines) {
          if ($l -match $rx) {
            $all += "$($Matches.a):$($Matches.m)->$($Matches.t)"
            if ($Matches.mp -notmatch '^0+$' -and $Matches.tp -notmatch '^0+$' -and $Matches.t -ne '(unbound)') { $bound += $Matches.a }
          }
        }
        @{ Pass = ($bound.Count -eq 5); Detail = "bound $($bound.Count)/5: $($all -join '; ')" }
      } }
    @{ Kind = 'LogCount';   Name = 'no debris array is skipped for want of a mesh'; Pattern = '\[debrispass\] array SKIPPED'; Max = 0 }
    @{ Kind = 'Script'; Name = 'debris DREW: a [debrispass] draw with d3d > 0 (draws D3D accepted)'; Script = {
        param($ctx)
        $rx = '^\[debrispass\] draw array=(?<a>\d) batches=(?<b>\d+) instances=(?<i>\d+) indices=(?<x>\d+) vertices=(?<v>\d+) d3d=(?<d>\d+)(?: us=(?<u>\d+))?$'
        $per = @{}
        foreach ($l in $ctx.LogLines) {
          if ($l -match $rx) {
            $a = [int]$Matches.a
            if (-not $per.ContainsKey($a)) { $per[$a] = @{ lines = 0; batches = 0; d3d = 0; us = 0; indices = [int]$Matches.x; vertices = [int]$Matches.v } }
            $per[$a].lines += 1; $per[$a].batches += [int64]$Matches.b; $per[$a].d3d += [int64]$Matches.d
            if ($Matches.u) { $per[$a].us += [int64]$Matches.u }
          }
        }
        $d3d = 0; $parts = @()
        foreach ($k in ($per.Keys | Sort-Object)) {
          $p = $per[$k]; $d3d += $p.d3d
          $parts += "array $k lines=$($p.lines) batches=$($p.batches) d3d=$($p.d3d) cpu=$([math]::Round($p.us / [math]::Max(1, $p.lines), 1))us/pass mesh $($p.indices)i/$($p.vertices)v"
        }
        @{ Pass = ($d3d -gt 0); Detail = "d3d draws $d3d; $($parts -join '; ')" }
      } }
    @{ Kind = 'LogCount';   Name = 'the stride-0 debris stream is drawn at its declaration''s extent'
       Pattern = '^\[stride0\] binder rva=0x[0-9A-F]+: stride-0 stream drawn at the bound declaration''s stream-0 extent 36 '; Min = 1 }
    @{ Kind = 'Script'; Name = 'exactly ONE binder reaches a fast-set draw with a stride-0 stream 0 (the debris mesh bind)'; Script = {
        param($ctx)
        $binders = @{}
        foreach ($l in $ctx.LogLines) {
          if ($l -match '^\[stride0\] DRAW reached \S+ with a stride-0 fast-set stream 0: binder rva=(?<r>0x[0-9A-F]+)') { $binders[$Matches.r] = $true }
          elseif ($l -match '^\[stride0\] tally: binder rva=(?<r>0x[0-9A-F]+) .* zero-draws=(?<d>\d+)$' -and [int64]$Matches.d -gt 0) { $binders[$Matches.r] = $true }
        }
        @{ Pass = ($binders.Count -eq 1); Detail = "binders with a stride-0 draw: $($binders.Count) [$(@($binders.Keys) -join ', ')]" }
      } }
  )
}
