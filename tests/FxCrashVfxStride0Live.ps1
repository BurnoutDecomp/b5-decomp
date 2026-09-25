# FX-CRASHVFX C3 (crash parity 2026-09-25) -- WHO DRAWS WITH A STRIDE-0 FAST-SET STREAM, live.
#
# The console's D3DDevice_SetStreamSource @0x8293D688 stores stride >> 2 per stream and raises the vfetch-patch
# dirty bit only when that is nonzero: a stride of 0 leaves the stride baked into the vertex shader's vfetch in
# force. renderengine::MeshHelper::Dispatch @0x8227B530 -- the debris renderer's mesh bind -- binds stream 0 with
# stride 0 (`li r7, 0`). The PC fast-set draw paths (WorldDraw_IndexedUP / WorldDraw_NonIndexedUP) used to SKIP a
# stride-0 stream; C3 gives stride 0 the console's meaning (the bound declaration's stream-0 extent). Before that
# change could land, this case had to show which callers it touches: every fast-set draw that ARRIVES with a
# stride-0 stream, per binder.
#
# The drive is the glass case's (FxCrashVfxGlassLive): boot, drive, one launch at 70 m/s into the glass wall at
# (3249.8, -3.7, -1925.4) -- a crash, glass debris and the crash-debris burst, with the sky, the Lion boost
# effects, the sparks and the simple particles all drawing.
# The witness (FLAG PC platform leaf, NOT IN THE X360 BINARY, default off, capped):
#   [stride0]  BRN_STRIDE0_DIAG=1 -- per binder (the return address of the stream-0 D3DDevice_SetStreamSource
#              that published the stash, as an RVA into Burnout_PC.map): its binds, its stride-0 binds of a
#              live buffer, and the fast-set draws that arrived with the stash's stride still 0.
# NO FRAME DUMP.
@{
  Name    = 'fxcrashvfx_stride0'
  Area    = 'vfx'
  Bug     = 'A fast-set draw whose stream 0 was bound with stride 0 must be counted per binder, so the C3 stride-0 meaning can be shown to reach only the debris mesh bind (MeshHelper::Dispatch).'
  Frames  = $false
  Run     = @{
    Drive           = $true
    MaxSeconds      = 75
    CrashSweep      = '3249.796,-3.7,-1925.404'
    CrashSweepShots = '225:70'
    CrashSweepArm   = 4
  }
  DiagEnv = 'BRN_STRIDE0_DIAG=1,BRN_GLASS_DIAG=1,BRN_DEBRIS_DIAG=1,BRN_DEBRIS_DIAG_ARRAY=4'
  Checks  = @(
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'the sweep fired (the car was placed and launched)'; Pattern = '\[sweep\] shot 0/'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the car crashed (a crash record opened)'; Pattern = '\[crash-exit\] OPENED crash record'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'the witness is armed and tallied at least once'; Pattern = '^\[stride0\] tally: \d+ binder'; Min = 1 }
    @{ Kind = 'Script'; Name = 'at most ONE binder reaches a fast-set draw with a stride-0 stream 0 (the debris mesh bind)'; Script = {
        param($ctx)
        $rx = '^\[stride0\] (?:DRAW reached \S+ with a stride-0 fast-set stream 0: binder|tally: binder) rva=(?<r>0x[0-9A-F]+)(?<rest>.*)$'
        $binders = @{}
        foreach ($l in $ctx.LogLines) {
          if ($l -match $rx) {
            $rva = $Matches.r; $rest = $Matches.rest
            if ($l -match '^\[stride0\] DRAW') { $binders[$rva] = $true }
            elseif ($rest -match 'zero-draws=(?<d>\d+)' -and [int64]$Matches.d -gt 0) { $binders[$rva] = $true }
          }
        }
        $unknown = @($ctx.LogLines | Where-Object { $_ -match '^\[stride0\] DRAW with a stride-0 fast-set stream 0 whose binder did not fit' }).Count
        $zb = @($ctx.LogLines | Where-Object { $_ -match '^\[stride0\] binder rva=' } | ForEach-Object { ($_ -split ' ')[2] })
        @{ Pass = ($binders.Count -le 1 -and $unknown -eq 0)
           Detail = "binders with a stride-0 draw: $($binders.Count) [$(@($binders.Keys) -join ', ')]; unknown $unknown; binders with a stride-0 bind of a live stream: $($zb.Count) [$($zb -join ' ')]" }
      } }
  )
}
