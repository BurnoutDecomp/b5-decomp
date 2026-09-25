# FX-POPUP (crash parity 2026-09-25, owner-requested): the junkyard car-select "DwnldTitleUp" popup.
# CarSelectVehicle::OnEnter @0x824C9470 posts the type-184 overlay request "DwnldTitleUp" on the first
# car select of a boot (the byte at this+0x414E latches it); the overlays director (b5 87ab2a75) shows
# it over the car-select screen. The owner's frame (exe 20:41:48 25/09/2026) shows the whole screen
# under a light haze and the popup's dark panel see-through.
#
# WHAT IS AUTHORED (console quirk, not tested here): the haze is FLAPTHUD clip 432 depth 1, an
# untextured white quad over the whole screen at CXForm alpha 0x6B (0.42), and the panel is dirt art
# with alpha holes plus a MenuGrad band at alpha 0.5 -- the console draws the same.
# WHAT WAS WRONG: the popup is FLAPT, drawn by the immediate Im2d path (CgsIm2d.cpp), which folded the
# colour transform's SHIFT into the vertex colour: texel * (vertex*scale + shift). The console adds it
# after the texel -- texel * (vertex*scale) + shift (Im2d program 0, Criterion's 0.fx/1.fx) -- and the
# popup's flat colours (RGB scale 0 + shift) sit on dark atlases, so the banner stripe drew
# (59,1,1) instead of (102,4,4) and the BlackDirt / RedDirt pieces drew black instead of 0.2 grey /
# (0.216,0.078,0.078).
# THE WITNESS: BRN_IM2D_TRACE's `[Im2dTrace]` line (every 60th present, one line per immediate draw)
# prints the vertex colour the device receives and `shift=RRGGBB`, the TFACTOR texture stage 1 adds
# (or '-' when the stage is off). The frame check reads the stripe's pixels in the last dumped frame.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxPopupDlcTitleUpdateLive.ps1
# RED on the pre-fix exe: -NoRun -RunDir scratch/bugtest/runs/fxpopup_dlc_title_update/20260925_213139 -ExpectFail
@{
    Name = 'fxpopup_dlc_title_update'
    Area = 'gui'
    Bug = 'The DwnldTitleUp popup''s flat colours must draw as the console does: texel * (vertex*scale) + shift, not texel * (vertex*scale + shift) -- the banner stripe (102,4,4), the BlackDirt 0.2 grey and the RedDirt (0.216,0.078,0.078), not near-black.'
    Frames = $true
    Run = @{
        SkipIntro = $true
        AcceptGap = 1.0
        HoldCarSelect = $true
        MaxSeconds = 35
        FrameEvery = 240
        MinFreeGB = 5
    }
    DiagEnv = 'BRN_IM2D_TRACE=1'
    Checks = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached car select'; Cue = 'carsel' }
        # The popup is up: its authored full-screen white wash, CXForm alpha 0x6B -> vertex alpha 0x6C.
        @{ Kind = 'LogMatch'; Name = 'the DwnldTitleUp popup drew (its authored white wash, alpha 0x6C)'
           Pattern = '\[Im2dTrace\] f=\d+ n=\d+ xy=\S+ rgba=FFFFFF6C '; Expect = $true }
        # FLAPTHUD clip 432 cxf shifts, read from the bundle: obj 12 (0.4,0.015686,0.015686) -> 660404,
        # obj 7/8 0.2 grey -> 333333, obj 11 (0.215686,0.078431,0.078431) -> 371414. Each rides texture
        # stage 1 with the vertex colour carrying only the scale (RGB scale 0 -> 000000).
        @{ Kind = 'LogMatch'; Name = 'the banner stripe''s flat colour is added after the texel (TFACTOR 660404)'
           Pattern = '\[Im2dTrace\] f=\d+ n=\d+ xy=\S+ rgba=000000[0-9A-F]{2} tex=\S+ shift=660404'; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'the BlackDirt grey is added after the texel (TFACTOR 333333)'
           Pattern = '\[Im2dTrace\] f=\d+ n=\d+ xy=\S+ rgba=000000[0-9A-F]{2} tex=\S+ shift=333333'; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'the RedDirt colour is added after the texel (TFACTOR 371414)'
           Pattern = '\[Im2dTrace\] f=\d+ n=\d+ xy=\S+ rgba=000000[0-9A-F]{2} tex=\S+ shift=371414'; Expect = $true }
        # THE DEFECT, as the device saw it: a flat colour folded into the vertex colour.
        @{ Kind = 'LogCount'; Name = 'no popup flat colour is folded into a vertex colour'
           Pattern = '\[Im2dTrace\] f=\d+ n=\d+ xy=\S+ rgba=(660404|333333|371414)FF '; Max = 0 }
        # The stripe on screen. Two models, both read from the image: the console draws the cxf shift,
        # 0.4 * 255 = 102 red; the old fold drew texel * shift = 148 * 0.4 = 59 (atlas 17's stripe texel
        # median red is 148). PASS when the measured median is nearer the console model. The pixels are
        # the saturated reds (R > 3*max(G,B), R > 20) inside the stripe draw's bounds (227,253)-(691,491)
        # from [Im2dTrace]: under the 42% white wash nothing else in that box is a saturated red.
        @{ Kind = 'Script'; Name = 'the banner stripe draws nearer the console colour than the old fold'
           Script = {
             param($ctx)
             $KF_CONSOLE_R = 0.4 * 255.0
             $KF_OLDFOLD_R = 148.0 * 0.4
             $laFrames = @(Get-ChildItem $ctx.FrameDir -Filter 'bb_*.bmp' -ErrorAction SilentlyContinue | Sort-Object Name)
             if ($laFrames.Count -eq 0) { return @{ Pass = $false; Detail = "no frames in '$($ctx.FrameDir)'" } }
             Add-Type -AssemblyName System.Drawing
             $lBmp = [System.Drawing.Bitmap]::FromFile($laFrames[-1].FullName)
             try {
               $lfSX = $lBmp.Width / 1280.0; $lfSY = $lBmp.Height / 720.0
               $lRect = New-Object System.Drawing.Rectangle ([int](227 * $lfSX)), ([int](253 * $lfSY)), ([int]((691 - 227) * $lfSX)), ([int]((491 - 253) * $lfSY))
               $lData = $lBmp.LockBits($lRect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
                                       [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
               $laBytes = New-Object byte[] ($lData.Stride * $lData.Height)
               [System.Runtime.InteropServices.Marshal]::Copy($lData.Scan0, $laBytes, 0, $laBytes.Length)
               $liW = $lData.Width; $liH = $lData.Height; $liStride = $lData.Stride
               $lBmp.UnlockBits($lData)
             } finally { $lBmp.Dispose() }
             $laRed = New-Object System.Collections.Generic.List[int]
             for ($y = 0; $y -lt $liH; $y++) {
               $liRow = $y * $liStride
               for ($x = 0; $x -lt $liW; $x++) {
                 $i = $liRow + 3 * $x
                 $liB = [int]$laBytes[$i]; $liG = [int]$laBytes[$i + 1]; $liR = [int]$laBytes[$i + 2]
                 $liM = [Math]::Max([Math]::Max($liG, $liB), 1)
                 if ($liR -gt 20 -and $liR -gt 3 * $liM) { $laRed.Add($liR) }
               }
             }
             if ($laRed.Count -lt 1000) { return @{ Pass = $false; Detail = "only $($laRed.Count) stripe pixels in $($laFrames[-1].Name) -- is the popup up?" } }
             $laRed.Sort()
             $liMedian = $laRed[[int]($laRed.Count / 2)]
             $lbPass = [Math]::Abs($liMedian - $KF_CONSOLE_R) -lt [Math]::Abs($liMedian - $KF_OLDFOLD_R)
             return @{ Pass = $lbPass; Detail = ("{0}: {1} stripe pixels, median R {2}; console model {3:f0}, old-fold model {4:f0}" -f $laFrames[-1].Name, $laRed.Count, $liMedian, $KF_CONSOLE_R, $KF_OLDFOLD_R) }
           } }
    )
}
