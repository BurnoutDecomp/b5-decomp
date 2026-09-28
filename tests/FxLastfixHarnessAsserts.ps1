# L3 RACEINTRO (2026-09-28): the organic harness's boot-order assert cascade. Only the two FX-LASTFIX live cases
# tolerate it.
# Every organic run (run_rival_organic.py, with --ai-pad or with the forced-takedown harness) asks for AI control of the
# player car at boot, before the game knows the player car. That fires "Unable to set the player car under AI control, as
# we don't know who they are yet" (BrnWorldModule.cpp:1544), and the race-car module then asserts in a cascade:
#   leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0   BrnRaceCarEntityModule.h:1835
#   IsAttached()                                       BrnRaceCarEntityModule.cpp:5311 and BrnActiveRaceCar.cpp:1060
#   lpPlayerCar                                        BrnRaceCarEntityModule.cpp:5317
# The cascade comes near log line 1370, before the junkyard (seen in fxlastfix_heading_ease/20260928_070325 and
# fxlastfix_ice_spaces/20260928_072030). It is caused by the harness's order, not by the ICE camera these cases witness
# (noted in scratch/OWNERLIST_0927/L3.md).
# This script points the case's NewAsserts check at a list made of the default known list PLUS those four families.
# Any other new family still fails the case.
#   $case = & (Join-Path $PSScriptRoot 'FxLastfixHarnessAsserts.ps1') $case
param($case)
$default = Join-Path $PSScriptRoot '..\..\tools\tests\known_asserts.txt'
$merged = Join-Path ([System.IO.Path]::GetTempPath()) ('brn_known_asserts_' + $case.Name + '.txt')
$lines = @()
if (Test-Path $default) { $lines += @(Get-Content $default) }
$lines += @(
    '# L3 2026-09-28: the organic harness boot-order cascade (b5-decomp/tests/FxLastfixHarnessAsserts.ps1)'
    '^Unable to set the player car under AI control, as we don''t know who they are yet'
    '^leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0$'
    '^IsAttached\(\)$'
    '^lpPlayerCar$'
)
Set-Content -Path $merged -Value $lines -Encoding ASCII
foreach ($check in $case.Checks) { if ($check.Kind -eq 'NewAsserts') { $check.Known = $merged } }
$case
