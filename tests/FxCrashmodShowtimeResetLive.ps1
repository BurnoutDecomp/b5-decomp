# FX-CRASHMOD (crash parity 2026-09-23), G64-D1 dispatch witness. In a Showtime mode the player's
# crash record opens with KF_PLAYER_SHOWTIME_CAR_RESET_SECONDS == 15.0 (flt_8300E9B0, CRT thunk
# 0x82C6AC28; read at 0x827CAEE0), not the .bss image value 0.0. ProcessCrashedRaceCarEvents' own
# [crash-exit] OPENED line prints the value. No gameplay claim: the seconds are consumed only while
# mbClearUpEnabled is set, which Showtime's KU_FLAG_DISABLE_CRASH_CLEAN_UP clears.
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'fxcrashmod_showtime_reset'
$case.Bug = 'The player''s Showtime crash record must open with 15.0 s (flt_8300E9B0), not 0.0.'
$case.Checks += @(
    @{ Kind = 'LogCount'; Name = 'Showtime player crash record opens with 15 s'
       Pattern = '\[crash-exit\] OPENED crash record for active race car 0 seconds=15\.0'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'no player crash record opens with 0 s'
       Pattern = '\[crash-exit\] OPENED crash record for active race car 0 seconds=0\.0'; Max = 0 }
)
$case
