# A labelled debug placement supplies the approach speed; the actual road
# triggers, elapsed time, scoring, plate update and autosave remain untouched.
$case = & (Join-Path $PSScriptRoot 'PlaytestRoadRuleEarnLive.ps1')
$case.Name = 'playtest_road_rule_win'
$case.Bug = 'A genuinely par-beating road run must save and produce a silver plate.'
$case.Run.CrashSweepShots = '9:0,9:80'
$case.Run.CrashSweepSettle = 300
# The first remote placement warms the streamed road. Wait for the original
# crash/reset state to recover; the old900-frame backstop fired during a crash.
$case.Run.CrashSweepMax = 3600
$case.Run.MaxSeconds = 85
$case.DiagEnv = $case.DiagEnv.Replace(',BRN_AI_PAD_PLAYER=cruise', '')
$case.DiagEnv += ',BRN_IM2D_MASK_DIAG=1'
$case.Checks = @($case.Checks | Where-Object { $_.Name -ne 'measured approach seated on road' })
$case.Checks += @(
    @{ Kind='Script'; Name='measured shot really reaches its launch at speed'; Script={
        param($ctx)
        $shot = -1; $seat = $false; $speed = $false
        $culture = [Globalization.CultureInfo]::InvariantCulture
        for ($i=0; $i -lt $ctx.LogLines.Count; ++$i) {
            $line = $ctx.LogLines[$i]
            if ($line -match '\[sweep\] shot 1/2 .* forced 0 wasCrashing 0') { $shot = $i }
            if ($shot -lt 0) { continue }
            if ($line -match '\[sweep\] seat ok shot 1 .* off ([0-9.]+) m') {
                # Three moving frames can travel several metres, but cannot
                # legitimately be275m from the request as in the failed control.
                $seat = [double]::Parse($Matches[1],$culture) -lt 10.0
            }
            if ($line -match '\[motion\].* pos ([-0-9.]+) ([-0-9.]+) ([-0-9.]+).*\|v\| ([0-9.]+)') {
                $x=[double]::Parse($Matches[1],$culture)
                $y=[double]::Parse($Matches[2],$culture)
                $z=[double]::Parse($Matches[3],$culture)
                $v=[double]::Parse($Matches[4],$culture)
                if ([Math]::Abs($x-1446.676) -lt 20 -and [Math]::Abs($y-3.287) -lt 5 -and
                    [Math]::Abs($z+2222.325) -lt 50 -and $v -gt 50) { $speed=$true }
            }
        }
        @{Pass=($shot -ge 0 -and $seat -and $speed); Detail="unforced/noncrashing shot=$($shot -ge 0), seat within10m=$seat, actual approach speed above50m/s=$speed"}
    } }
)
$case
