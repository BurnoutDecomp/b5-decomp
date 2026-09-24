# FX-BRIDGES CC-9 live case: the Showtime intro takes the car away from the pad. While
# GameStateModule::IsInShowtimeIntro() (the 0.5 s window DetectModeStarts opens when both bumpers are held),
# BrnGameModule::BridgeControllerToWorld @0x823CD890 publishes full throttle + full handbrake, no brake and the
# latched +/-1 steering (0x823CDB80..0x823CDBCC) -- the spin into Showtime. Before CC-9 the block was a comment and
# the player kept driving through the window.
#
# The drive is ShowtimeContacts.ps1 unchanged (fresh profile, both-bumpers Showtime entry at 40 s) with
# BRN_PROP_DIAG=1, the gate of the showtime arm's own witnesses:
#   [showtime] INTRO OPEN -- ... steering=<+/-1> window=0.5s          GameStateModule_gSR_00.cpp (the window opens)
#   [showtime-intro] BridgeControllerToWorld override: accel 1.000000 brake 0.000000 handbrake 1.000000 steering <+/-1.000000>
#                                                                     GameBridgeControllerToX.cpp (the override ran)
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxBridgesShowtimeIntroLive.ps1
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'fxbridges_showtime_intro'
$case.Bug = 'During the Showtime intro the controller bridge must publish full throttle + handbrake and the latched steering, not the pad.'
$case.DiagEnv += ',BRN_PROP_DIAG=1'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'CC-9: the Showtime intro window opened (both bumpers, gate passed)';
       Pattern = '\[showtime\] INTRO OPEN'; Expect = $true }
    @{ Kind = 'Script'; Name = 'CC-9: the controller bridge overrides the pad during the intro (accel 1, brake 0, handbrake 1, +/-1 steering)'; Script = {
        param($ctx)
        $opens = @($ctx.LogLines | ForEach-Object {
            if ($_ -match '\[showtime\] INTRO OPEN.* steering=([-+0-9.eE]+)') { [double]::Parse($Matches[1], [cultureinfo]::InvariantCulture) }
        })
        $over = @($ctx.LogLines | Where-Object { $_ -match '\[showtime-intro\] BridgeControllerToWorld override:' })
        $good = @($over | Where-Object { $_ -match 'accel 1(\.0+)? brake 0(\.0+)? handbrake 1(\.0+)? steering (-?1)(\.0+)?(\s|$)' })
        $steers = @($good | ForEach-Object { if ($_ -match 'steering (-?1)(\.0+)?') { [double]$Matches[1] } } | Select-Object -Unique)
        $match = ($opens.Count -gt 0 -and $steers.Count -gt 0 -and ($steers | Where-Object { $opens -contains $_ }).Count -gt 0)
        @{ Pass = ($good.Count -gt 0 -and $good.Count -eq $over.Count -and $match)
           Detail = "$($over.Count) override line(s), $($good.Count) with the console's forced values; steering $($steers -join ',') vs intro latch $($opens -join ',')" }
    } }
)
$case
