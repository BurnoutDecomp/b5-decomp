# Established land wall crash; crumple must reach the mixer during impact time.
$case = & (Join-Path $PSScriptRoot 'L1CamCollideCrashLive.ps1')
$pcm = & (Join-Path $PSScriptRoot 'FxRoadSurfaceAudioLive.ps1')
$case.Name = 'fx_crumple_audio'
$case.Bug = 'The deformation effect must derive crumple intensity from live sensors during impact time.'
# Returning profile starts at the nearby original junkyard; the runner restores slot 5.
$case.ProfileFixture = 'scratch/OWNERLIST_0927/CODEX/FreshGlassSlot5.sav'
$case.PcmTraceFile = $pcm.PcmTraceFile
$case.Run.SkipIntro=$true
$case.Run.AcceptGap=1.0
$case.Run.MaxSeconds=75
$case.Run.MotionProbe=$true
$case.Run.CrashSweep='3249.796,-3.7,-1925.404'
$case.Run.CrashSweepShots='225:70'
$case.Run.CrashSweepSettle=600
$case.DiagEnv = 'BRN_CRASH_RESPONSE_DIAG=1,BRN_SNDENV_DIAG=1,BRN_HUD_SOUND_DIAG=1,BRN_CRASHCAM_DIAG=1,BRN_COLLISION_AUDIO_DIAG=1,BRN_SWEEP_WAIT_ROAMING=1,BRN_ULTRA_SLOMO_SCALE=0.0075,BRN_SOUND_PCM_TRACE=' + (Split-Path $case.PcmTraceFile -Leaf)
$case.Checks = @(
    @{Kind='LogCount'; Name='fixture car is PUSMC01'; Pattern='STRM: Adding racecar for streaming: car=0, model=VEH_PUSMC01 '; Min=1}
    @{Kind='LogCount'; Name='no unexpected player vehicle'; Pattern='STRM: Adding racecar for streaming: car=0, model=(?!VEH_PUSMC01 )'; Max=0}
    @{Kind='LogCount'; Name='wall shot seated successfully'; Pattern='\[sweep\] seat ok shot 0 '; Min=1}
    @{Kind='Mark'; Name='reached DRIVING'; Phase='DRIVING'}
    @{Kind='LogCount'; Name='real crash entered'; Pattern='\[crashcam\] container current state -> 2 \(ArbStateCrashing\)'; Min=1}
    @{Kind='LogCount'; Name='original ultra impact moment runs'; Pattern='\[crashcam\] hardstop ALLOCATED .* ultra 1 '; Min=1}
    @{Kind='LogCount'; Name='crash shot seated on the road'; Pattern='\[sweep\] SEAT BAD shot 0'; Max=0}
    @{Kind='LogCount'; Name='deformation data reached sound'; Pattern='^\[collision-audio\] deform queues '; Min=1}
    @{Kind='LogCount'; Name='zero assertions'; Pattern='\[ASSERT \d+\]'; Max=0}
    @{Kind='LogCount'; Name='zero exceptions'; Pattern='\[EXCEPTION\]'; Max=0}
)
$case.Checks += @{
    Kind='Script'; Name='crumple samples reach the dry mix'
    Script={
        param($ctx)
        $saved = Join-Path $ctx.RunDir 'crumple_pcm_trace.txt'
        if (-not (Test-Path -LiteralPath $saved)) {
            Copy-Item -LiteralPath $ctx.Case.PcmTraceFile -Destination $saved
        }
        $banks=@{}; $samples=@{}; $mixed=0
        foreach ($line in Get-Content -LiteralPath $saved) {
            if ($line -match '^aems-bank bank=(\d+) .*path=.*CrumplePatchBank\.abi') {
                $banks[$Matches[1]]=$true
            } elseif ($line -match '^aems-select bank=(\d+) sample=(\w+) index=\d+$') {
                $samples[$Matches[2]]=$Matches[1]
            } elseif ($line -match '^aems-mix-pcm .*sample=(\w+) .* peak=([^ ]+)$') {
                $sample=$Matches[1]
                $peak=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture)
                if ($samples.ContainsKey($sample) -and $banks.ContainsKey($samples[$sample]) -and $peak -gt 0) { $mixed++ }
            }
        }
        @{Pass=($mixed -gt 0); Detail="$mixed nonzero crumple Send contributions"}
    }
}
$case
