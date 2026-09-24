# FX-FLOW (crash parity 2026-09-24): the drive-thru PAINT SHOP, live.
# Before: every paint shop fired "Can not instance resource pointer - it has no main memory resource"
# (CgsResourcePtr.h) from DriveThruManager::ProcessDriveThru and then faulted reading +0x20 of null
# (scratch/bugtest/runs/fxtraffic_crash_release/20260923_152313, BrnGame.log:59737) -- the
# GameStateModule never bound its "CarColours" palette (Prepare stages 11/12, @0x8239E9D4..0x8239EA90).
# The car is placed ~35 m short of the paint shop at (2907.3, -1595.2) (generic region 241694, the
# one the pursuit runs crossed), on the road heading into it (the pursuit car's own velocity there,
# (28.3, 0.2, -19.6) m/s == 125 deg clockwise from +Z), and drives straight in.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxFlowPaintShop.ps1
@{
    Name = 'fxflow_paint_shop'
    Area = 'drivethru'
    Bug = 'A paint shop must read the GameStateModule palette bound by Prepare stages 11/12 (CarColours, pool 5) and post action 98, not assert "Can not instance resource pointer" and fault.'
    Frames = $false
    Run = @{
        Drive = $true
        MotionProbe = $true
        SkipIntro = $true
        AcceptGap = 1.0
        Teleport = '2878.5,1.4,-1575.3,125'
        SkipTrainingTip = $true
        MaxSeconds = 75
    }
    DiagEnv = 'BRN_TRAFFIC_DIAG=1'
    Checks = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'no "Can not instance resource pointer" assert'; Pattern = 'Can not instance resource pointer'; Max = 0 }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
        @{ Kind = 'LogMatch'; Name = 'Prepare stage 12 bound the CarColours palette'; Pattern = '\[GameStateModule::Prepare\] stage 12 -- "CarColours" bound: 1 \(paint-shop palette 2 colours [1-9]\d*\)'; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'the player entered the paint shop'; Pattern = '\[drivethru\] ENTER type=3 '; Expect = $true }
        @{ Kind = 'LogMatch'; Name = 'the paint arm read the palette and posted action 98'; Pattern = '\[drivethru\] POST paint action=98 colour=\d+ palette=2 \(palette colours [1-9]\d*\)'; Expect = $true }
    )
}
