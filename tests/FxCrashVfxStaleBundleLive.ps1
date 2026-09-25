# FX-CRASHVFX C3 (crash parity 2026-09-25, CC-15) -- A PRE-PORT PARTICLES.BUNDLE IS TOLERATED, live.
#
# Players convert their own game data. A PARTICLES.BUNDLE converted before the debris-mesh port carries its three
# VFXMeshCollection resources BIG-ENDIAN; with 0x10019 registered, the console's FixUp walk would assert on the
# version and then follow a byte-reversed MeshHelper offset (the pre-C3 FixUp access-violates on such a header:
# run_fxcrashvfx_mesh_fixup.py). The FLAG PC platform leaf in BrnVFXMeshCollectionResourceType recognises the
# byte-reversed version word, leaves the collection untouched and says so ONCE with the re-conversion command;
# LoadFXBundle stage 6 does not bind it and stage 7 asks nothing for it -- the debris stays undrawn, as before the
# port, and the game runs on.
#
# !! THIS CASE NEEDS THE PRE-PORT BUNDLE INSTALLED: run it through the wrapper that swaps
#   scratch/CRASHPARITY_0922/fixes/FX-CRASHVFX.asset_backup/PARTICLES.BUNDLE (sha256 469bb719...) into build/game
#   under the box lock and swaps the ported one back afterwards. On a ported bundle it FAILS (no refusal line).
# The drive is the glass case's (a crash into the glass wall at 70 m/s: crash debris and glass debris spawn).
# Witnesses (NOT IN THE X360 BINARY): the refusal line (always on, once); [fxbundle] / [debrispass] BRN_DEBRIS_DIAG=1.
# NO FRAME DUMP.
@{
  Name    = 'fxcrashvfx_stale_bundle'
  Area    = 'vfx'
  Bug     = 'An exe that registers the debris mesh type must run on a PARTICLES.BUNDLE converted before the mesh port: refuse the big-endian collections with one line, bind nothing, draw no debris, and never assert or fault.'
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
    @{ Kind = 'LogCount';   Name = 'the refusal said so exactly ONCE, naming the bundle and the re-conversion command'
       Pattern = '^\[particles\] PARTICLES\.BUNDLE holds an UNCONVERTED \(big-endian\) debris mesh collection.*--only PARTICLES\.BUNDLE'; Min = 1; Max = 1 }
    @{ Kind = 'LogCount';   Name = 'the FX-bundle ladder still finished (no stall)'; Pattern = '\[skid-ready\] LoadFXBundle DONE'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'no debris array bound a mesh ([fxbundle] ... texture ''(unbound)'')'
       Pattern = '^\[fxbundle\] stages 5-8 done: debris array \d ''[^'']+'' mesh=0+ texture ''\(unbound\)'' -> 0+$'; Min = 5 }
    @{ Kind = 'LogCount';   Name = 'the debris stays undrawn, as before the port (array SKIPPED)'; Pattern = '\[debrispass\] array SKIPPED'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'no debris draw'; Pattern = '^\[debrispass\] draw array='; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no fast-set draw with a stride-0 stream'; Pattern = '^\[stride0\] DRAW reached'; Max = 0 }
  )
}
