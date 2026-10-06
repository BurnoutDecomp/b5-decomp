# Enter a real offline race at the existing tested junction, then open CrashNav
# through the pad. Frame review must check start, finish and opponent markers;
# successful menu input alone does not establish their presence.
$case = & (Join-Path $PSScriptRoot 'FxScenariosModeIntroRaceLive.ps1')
$case.Name = 'playtest_full_map_race'
$case.Area = 'ui/satnav'
$case.Bug = 'Full-screen map must retain race start, finish and opponent markers during an event (issue 33).'
$case.ProfileFixture = 'rival_hunt_profile.sav'
$case.Frames = $true
$case.Run.MaxSeconds = 90
$case.Run.FrameEvery = 60
$case.Run.MenuScript = "timeout:45;wait:\[stunt\] mode state -> E_GMS_IN_PROGRESS;sleep:2;tap:PauseMap;wait:\[screen\] ENTER 'CN_MAP_MAIN;sleep:6;mark:race-map-initial;hold:GuiZoom:1.5;sleep:3;mark:race-map-zoomed;tap:DPadRight;sleep:3;tap:DPadLeft;sleep:3;mark:race-map-filter-return;tap:Stop"
$case.DiagEnv += ',BRN_SCREEN_DIAG=1,BRN_SATNAV_DIAG=1,BRN_HUD_VIS=1,BRN_FRAME_DUMP_MAX=230'
$case.Checks += @(
    @{ Kind='LogCount'; Name='no assertions'; Pattern='\[ASSERT(?:\s|\])'; Max=0 }
    @{ Kind='LogMatch'; Name='map opens during the race'; Pattern="\[screen\] ENTER 'CN_MAP_MAIN" }
    @{ Kind='Script'; Name='race map input sequence completes'; Script={
        param($ctx)
        @{Pass=($ctx.MarksText -match 'mark race-map-initial' -and $ctx.MarksText -match 'mark race-map-zoomed' -and $ctx.MarksText -match 'mark race-map-filter-return'); Detail='Actual race, direct map entry, zoom and filter return completed; marker rendering requires frame review.'}
    } }
)
$case
