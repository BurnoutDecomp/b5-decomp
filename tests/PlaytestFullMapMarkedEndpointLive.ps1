# Observe the native finish device position before deciding whether a pan is
# needed. The 30s observation window permits ordinary existing right-stick
# CameraLeft/Right/Up/Down channels; never change a camera or map transform.
$case = & (Join-Path $PSScriptRoot 'PlaytestFullMapEventMarkersLive.ps1') -Mode MarkedMan
$case.Name = 'playtest_full_map_marked_endpoint'
$case.Bug = 'Observe the actual MarkedMan start/finish renderer input, then verify an offscreen finish through original map panning.'
$case.Run.MinFreeGB = 20
$case.Run.MenuScript = "timeout:50;wait:\[stunt\] mode state -> E_GMS_IN_PROGRESS;sleep:5;tap:PauseMap;wait:\[screen\] ENTER 'CN_MAP_MAIN;sleep:6;mark:endpoint-map-initial;hold:GuiZoom:3;sleep:3;mark:endpoint-map-wide;sleep:30;mark:endpoint-observed;tap:Stop"
$case.DiagEnv = 'BRN_SCREEN_DIAG=1,BRN_SATNAV_DIAG=1,BRN_HUD_VIS=1,BRN_INTRO_TIMER_DIAG=1,BRN_PROP_DIAG=1,BRN_MM_DIAG=1,BRN_FRAME_DUMP_MAX=210'
# This case observes/pans one endpoint rather than repeating filter changes.
$case.Checks = @($case.Checks | Where-Object {$_.Name -ne 'initial, wide and restored event filters were captured'})
$case.Checks += @(
    @{Kind='Script';Name='native finish renderer receives a finite endpoint quad';Script={
        param($ctx)
        $count=0; $bad=0; $inv=[cultureinfo]::InvariantCulture
        foreach($line in $ctx.LogLines) {
            if($line -notmatch '^\[cnav-endpoint\] icon=\d+ state=52 position=(?<position>[^ ]+) rect=(?<rect>[^ ]+)') {continue}
            $count++
            foreach($s in ($Matches.position.Split(',')+$Matches.rect.Split(','))) {
                $v=[double]::Parse($s,$inv)
                if([double]::IsNaN($v)-or[double]::IsInfinity($v)) {$bad++}
            }
        }
        @{Pass=($count -ge 1 -and $bad -eq 0);Detail="native finish quad rows=$count, nonfinite=$bad; compare viewport/pan pixels separately"}
    }}
    @{Kind='Script';Name='endpoint observation window completed';Script={
        param($ctx)
        @{Pass=($ctx.MarksText -match 'mark endpoint-map-wide' -and $ctx.MarksText -match 'mark endpoint-observed');Detail='Original event, direct map and held LT reached the bounded observation/pan window.'}
    }}
)
$case
