# One observed active maximum case. Uses existing default-off observers only.
# The source readback is the real pre-final-composite texture, with no UI/draw
# suppression. A written source is necessary evidence; it is not shape parity.
$case = & (Join-Path $PSScriptRoot 'PlaytestMaxDeformationActiveLive.ps1')
$case.Name = 'playtest_max_deformation_parts'
$case.Bug = 'Tie maximum panel bounds/hinge promotion and submitted wheel scale to actual paused world pixels.'
$case.DiagEnv += ',BRN_DEFORM_TRACE=1,BRN_RESLOADED_DIAG=1,BRN_WHEEL_DIAG=1,BRN_POSTFX_SOURCE_DUMP=1'
$case.Checks += @(
    @{ Kind='LogCount'; Name='actual skinned panel collision boxes observed'; Pattern='^\[part-box\]'; Min=1 }
    @{ Kind='LogCount'; Name='actual hinge geometry promoted'; Pattern='^\[hinge-geom\]'; Min=1 }
    @{ Kind='LogCount'; Name='original wheel scales delivered from the resource'; Pattern='^\[res-loaded\]'; Min=1 }
    @{ Kind='LogCount'; Name='actual precomposite world source captured'; Pattern='^\[postfx-source\] .*unit=0 .*written=1 '; Min=1 }
    @{ Kind='Script'; Name='world source captured inside actual Driver Details pause'; Script={
        param($ctx)
        $inside=$false; $frames=@()
        foreach($line in $ctx.LogLines) {
            if($line -match "^\[screen\] ENTER 'CN_D_DETAIL") { $inside=$true }
            elseif($line -match "^\[screen\] ENTER 'INGAME") { $inside=$false }
            if($inside -and $line -match '^\[postfx-source\] present=(?<p>\d+) unit=0 .*written=1 ') { $frames += [int]$Matches.p }
        }
        @{ Pass=($frames.Count -gt 0); Detail="actual paused source presents=$($frames -join ','); visual panel assessment remains separate" }
    } }
    @{ Kind='Script'; Name='precomposite source images saved'; Script={
        param($ctx)
        $files = @(Get-ChildItem -LiteralPath (Join-Path $ctx.FrameDir 'inputs') -Filter 'source_*.bmp' -ErrorAction SilentlyContinue)
        @{ Pass=($files.Count -gt 0); Detail="actual source images=$($files.Count); paused-frame correlation and visual inspection are separate" }
    } }
)
$case
