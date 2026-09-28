# OWNERLIST 2026-09-28, lane L6 -- THE DRIVER DETAILS SCREEN (owner: "Lot of Driver details are missing or wrong").
#
# The console chain (ARTIST):
#   START in free roam -> CN_D_DETAIL -> CrashNavDriverDetails::UpdateInitSetup posts GUI 435
#   -> BridgeGuiToGameState -> game event 79 -> ProgressionManager::GetGameStats @0x8238A6A0 fills a GameStats record
#   -> game action 180 -> TranslateGameActionsToGuiEvents case 180 @0x823EC8A0 builds GuiEventStatsResponse (GUI 436)
#   -> CrashNavDriverDetails::HandleStatData @0x824B8618 formats the 33 stat fields and the 3 x 5 district fields.
# Two PC defects on that chain, both visible on the slot-0 profile copy this case seeds (sha256 b8e67e19ef39):
#   * CARS OWNED read 171/86: GetGameStats' CARS_COLLECTED loop counted the colour / silver livery variants
#     (livery byte in {1,3,4}); the console counts the race cars whose livery is NOT in that set
#     (@0x8238A838..0x8238A888). Expected on this profile: 70 of 86.
#   * BEST AIR TIME / BEST SPIN showed the IEEE bits of the two floats (7.43 s -> 1089330688, 373.1 deg -> 1136300654):
#     HandleStatData reads +0xF4 with lfs (@0x824B8968) and +0xF8 with lfs + fctiwz (@0x824B8978).
#
# The run: stage an exe into slot 7 and seed Memcard_7 with the slot-0 copy first:
#   bash scratch/OWNERLIST_0927/L6/stage_slot7_dd.sh scratch/OWNERLIST_0927/L6/exes/<sha>
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/DriverDetailsLive.ps1 -Slot 7
# A returning boot, the car parked; START opens Driver Details at DRIVING+20 s (the licence tab), GUI_DOWN at +30 s
# (records) and +40 s (discover), GUI_CANCEL at +52 s. One frame per 60 presents for the BEFORE / AFTER pair.
# Witnesses:
#   BRN_SCREEN_DIAG `[screen] ENTER 'CN_D_DETAIL'`       -- the pause screen opened
#   `[ddetails] action 180 -> gui 436 (cars X/Y ...)`    -- the bridge rung (unconditional)
#   BRN_SCREEN_DIAG `[ddetails] stat text ...`           -- the formatted text HandleStatData left in each field
@{
  Name    = 'driver_details'
  Area    = 'gui'
  Bug     = 'Driver Details: CARS OWNED counted the colour/silver variants (171/86 on the slot-0 copy, console 70/86), and BEST AIR TIME / BEST SPIN printed the raw float bits.'
  Frames  = $true
  Run     = @{
    SkipIntro    = $true
    AcceptGap    = 1.0
    MaxSeconds   = 120
    PauseAt      = '20'
    PauseTarget  = 'driver'
    MenuTapAt    = '30:Next,40:Next'
    UnpauseAt    = '52'
    FrameEvery   = 60
  }
  DiagEnv = 'BRN_SCREEN_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';     Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the Driver Details pause screen opened (CN_D_DETAIL)'; Pattern = "\[screen\] ENTER 'CN_D_DETAIL\s*'"; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the stats response was built (action 180 -> GUI 436)'; Pattern = '\[ddetails\] action 180 -> gui 436'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'CARS OWNED is the console count on the slot-0 copy (70 of 86, @0x8238A838..0x8238A888)'
       Pattern = '\[ddetails\] action 180 -> gui 436 \(cars 70/86 '; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'carsWon_cpt shows 70 of 86'; Pattern = "\[ddetails\] stat text .*carsWon_cpt='70[^0-9]+86'"; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'bestAirTime_cpt shows the 7.43 s float, not its bits (lfs @0x824B8968)'
       Pattern = "\[ddetails\] stat text .*bestAirTime_cpt='[^']*7[.,]43"; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'bestSpin_cpt shows 373 (fctiwz of 373.14 @0x824B8978), not 1136300654'
       Pattern = "\[ddetails\] stat text .*bestSpin_cpt='[^']*\b373\b"; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'no field shows a float bit pattern (a 10-digit / billion-sized number, or a ".-" fraction)'
       Pattern = "\[ddetails\] stat text [^=]*='[^']*([0-9]{10}|[0-9]{1,3}(,[0-9]{3}){3}|\.-)"; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'the stat-text witness fired (7 sampled fields across the stat and district arrays)'
       Pattern = "\[ddetails\] stat text (hoursPlayed|bestAirTime|bestSpin|carsWon|bbPBH|jmpDTP|smhSL)_cpt="; Min = 7 }
  )
}
