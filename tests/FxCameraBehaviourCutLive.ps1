# Camera identity changes must reset finaliser inertia on the first countdown frame.
# MainDirector::Update, ARTIST 0x822745D0..0x82274628. The old race-intro case
# tolerated its one-frame 440 m jump; this case explicitly rejects that frame.
$case = & (Join-Path $PSScriptRoot 'FxRaceIntroTrafficLightSpaceLive.ps1')
$case.Name = 'fxcamera_behaviour_cut'
$case.Bug = 'The first countdown frame must stay at the race junction instead of blending toward a stale camera position.'
$case.Checks += @{ Kind = 'Script'; Name = 'the first countdown camera is within 35 m of the player'; Script = {
    param($ctx)
    $car = $null
    $countdown = $false
    $inv = [cultureinfo]::InvariantCulture
    foreach ($line in $ctx.LogLines) {
        if ($line -match '^\[tl-space\].*\| player ([-\d.eE+]+),([-\d.eE+]+),([-\d.eE+]+)') {
            $car = @(1..3 | ForEach-Object { [double]::Parse($Matches[$_], $inv) })
        }
        if ($line -match 'Prepare guid 574883|\[ice-prepare\] guid 574883') { $countdown = $true }
        if ($countdown -and $car -and $line -match '^\[cam\] f=(\d+) pos=([-\d.eE+]+),([-\d.eE+]+),([-\d.eE+]+) ') {
            $frame = $Matches[1]
            $eye = @(2..4 | ForEach-Object { [double]::Parse($Matches[$_], $inv) })
            $d = [math]::Sqrt([math]::Pow($eye[0]-$car[0],2) + [math]::Pow($eye[1]-$car[1],2) + [math]::Pow($eye[2]-$car[2],2))
            return @{ Pass = ($d -lt 35.0); Detail = ('First countdown camera f={0}: {1:0.00} m from player' -f $frame,$d) }
        }
    }
    @{ Pass = $false; Detail = 'No first countdown camera with a recorded junction/player position.' }
} }
$case
