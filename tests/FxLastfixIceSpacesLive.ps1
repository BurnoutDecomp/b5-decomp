# FX-LASTFIX item 1b (crash parity 2026-09-26) live case: an ICE take reads its OWN reference spaces.
#
# BehaviourIceAnim::Update @0x82247108 copies the MainDirector's shared ICE::CameraSpaceHandler (0x82247384) and
# overrides four of the COPY's spaces before its take evaluator reads it: CAR <- the behaviour's primary vehicle
# (0x82247388..), CAR2 <- its secondary vehicle (0x822473D0..), HEADING2 <- its eased heading space (0x822473DC..) and,
# under mbForceHeadingSpaceToBeLooseHeadingSpace (0x82247450), HEADING <- the player's loose heading space. The PC wrote
# none of them, so every take read the shared handler: CAR = the player, CAR2 = the race car NEAREST the player,
# HEADING2 = that car's raw transform -- and TAKEDOWN / REVERSE_TAKEDOWN (look-ats between CAR and CAR2) and BYSTANDER
# (CAR's arm) with them.
# This case is FxDirectorTakedownCamForced.ps1 (the reversed force-takedown harness; ArbStateCrashing starts the
# authored taken-down ICE camera with the KILLER as the secondary vehicle). The taken-down take Takendown (guid 575630)
# looks from CAR to CAR2 (eye TAKEDOWN) at CAR2 (look CAR2), so it frames the killer only through these writes.
# Witness (NOT X360, BRN_CRASHCAM_DIAG, armed by RivalOrganic): per take, frames 0..5, 96 lines a run at most:
#   [iceanim] spaces take G 'name' frame N: eye E look L | CAR primary id P: take T m, shared S m | CAR2 secondary id Q:
#     take T m, shared S m | HEADING2 yaw take Y, shared R, secondary Z deg | forced loose F
# "take" / "shared" are how many metres the take's copy / the shared handler put that space off the behaviour's own
# vehicle. With the fix every "take" is 0.00; "shared" says how far the old code was off (0 when the killer happened to
# be the nearest race car). HEADING2 "take" is the eased heading space, "shared" the nearest car's raw transform.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxLastfixIceSpacesLive.ps1 --run-name fxlastfix_ice_spaces
$case = & (Join-Path $PSScriptRoot 'FxDirectorTakedownCamForced.ps1')
$case.Name = 'fxlastfix_ice_spaces'
$case.Bug = 'An ICE take must read its behaviour''s own CAR / CAR2 / HEADING2 spaces (the primary vehicle, the secondary vehicle such as the killer, the eased heading space), as BehaviourIceAnim::Update writes them into its copy of the shared handler -- not the player / the nearest race car.'
$case.Checks += @(
    @{ Kind = 'LogCount'; Name = 'item 1b: the spaces witness fired'; Pattern = '^\[iceanim\] spaces take '; Min = 1 }
    @{ Kind = 'Script'; Name = 'item 1b: the take''s CAR / CAR2 are the behaviour''s primary / secondary on every witnessed frame, and a take that reads CAR2 / TAKEDOWN / REVERSE_TAKEDOWN / HEADING2 ran'; Script = {
        param($ctx)
        $inv = [cultureinfo]::InvariantCulture
        $all = @($ctx.LogLines | Where-Object { $_ -match '^\[iceanim\] spaces take ' })
        $rows = @($all | ForEach-Object {
            if ($_ -match '^\[iceanim\] spaces take (-?\d+) ''([^'']*)'' frame (\d+): eye (-?\d+) look (-?\d+) \| CAR primary id (\d+): take ([-\d.]+) m, shared ([-\d.]+) m \| CAR2 secondary id (\d+): take ([-\d.]+) m, shared ([-\d.]+) m \| HEADING2 yaw take ([-\d.]+), shared ([-\d.]+), secondary ([-\d.]+) deg \| forced loose (\d)') {
                [pscustomobject]@{ Guid = $Matches[1]; Take = $Matches[2]; Frame = [int]$Matches[3]
                                   Eye = [int]$Matches[4]; Look = [int]$Matches[5]
                                   CarTake = [double]::Parse($Matches[7], $inv); CarShared = [double]::Parse($Matches[8], $inv)
                                   Car2Take = [double]::Parse($Matches[10], $inv); Car2Shared = [double]::Parse($Matches[11], $inv) }
            }
        })
        $off = @($rows | Where-Object { $_.CarTake -gt 0.01 -or $_.Car2Take -gt 0.01 })
        $users = @($rows | Where-Object { @(4, 6, 8, 12) -contains $_.Eye -or @(4, 6, 8, 12) -contains $_.Look })
        $takes = @($rows | ForEach-Object { "$($_.Take) ($($_.Guid)) eye $($_.Eye) look $($_.Look)" } | Select-Object -Unique)
        @{ Pass = ($rows.Count -gt 0 -and $rows.Count -eq $all.Count -and $off.Count -eq 0 -and $users.Count -gt 0)
           Detail = ("{0} witness line(s) ({1} parsed), {2} with CAR / CAR2 off the behaviour's vehicle, {3} from a take that reads CAR2 / TAKEDOWN / REVERSE_TAKEDOWN / HEADING2; takes: {4}" -f
                     $all.Count, $rows.Count, $off.Count, $users.Count, ($takes -join ', ')) }
    } }
    @{ Kind = 'Script'; Name = 'item 1b: what the shared handler would have given (report: CAR2 metres off the killer, HEADING2 eased vs raw)'; Script = {
        param($ctx)
        $lines = @($ctx.LogLines | Where-Object { $_ -match '^\[iceanim\] spaces take ' } | Select-Object -First 8 |
                   ForEach-Object { $_ -replace '^\[iceanim\] spaces take ', '' })
        @{ Pass = $true; Detail = ($lines -join ' || ') }
    } }
)
$case
