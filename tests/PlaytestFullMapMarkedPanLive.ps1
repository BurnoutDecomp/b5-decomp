# Batch24 measured finish52 at viewport-top Y122.2; held LT brings X to599.3.
# Keep the original zoom-out held while the ordinary right stick pans up/left,
# moving the edge-clamped flag into the actual map area. No transform override.
$case = & (Join-Path $PSScriptRoot 'PlaytestFullMapMarkedEndpointLive.ps1')
$case.Name = 'playtest_full_map_marked_pan'
$case.Bug = 'Original right-stick panning must reveal the real MarkedMan finish inside the map mask.'
$case.Run.MenuScript = "timeout:50;wait:\[stunt\] mode state -> E_GMS_IN_PROGRESS;sleep:5;tap:PauseMap;wait:\[screen\] ENTER 'CN_MAP_MAIN;sleep:4;mark:endpoint-map-initial;hold:GuiZoom:18;sleep:4;mark:endpoint-map-wide;hold:CameraUp:2;sleep:2.3;hold:CameraLeft:0.6;sleep:2;mark:endpoint-pan;sleep:5;mark:endpoint-observed;sleep:5;tap:Stop"
$case.Checks += @(
    @{Kind='LogMatch';Name='ordinary right-stick vertical map pan received';Pattern='\[harness-camera\] axes 0 1 held 1'}
    @{Kind='LogMatch';Name='ordinary right-stick horizontal map pan received';Pattern='\[harness-camera\] axes -1 0 held 1'}
    @{Kind='Script';Name='finish quad moves down from the measured top edge';Script={
        param($ctx)
        $rows=0; $maxY=0.0; $inv=[cultureinfo]::InvariantCulture
        foreach($line in $ctx.LogLines) {
            if($line -match '^\[cnav-endpoint\] icon=\d+ state=52 position=[^, ]+,(?<y>[^ ]+) rect=') {
                $rows++;$maxY=[math]::Max($maxY,[double]::Parse($Matches.y,$inv))
            }
        }
        @{Pass=($rows -ge 2 -and $maxY -gt 180.0);Detail="finish rows=$rows, peak device Y=$maxY versus measured top122.2; actual mask/flag pixels require review"}
    }}
)
$case
