# FX-RCEM4 (crash parity 2026-09-24, CC-3 = G60-D3) -- a deformed race car's dynamic scene volume is
# replaced by its deformed box. One Road Rage run (RivalOrganic: the pursuit rams rivals, so bodies
# deform) with BRN_PROP_BOX_DIAG set: RaceCarEntityModule::UpdatePropBoundingBoxes_PreScene (ARTIST
# 0x822F5668, called from PreSceneUpdate 0x8230E408) must post InSceneUpdateInterface::
# ReplaceDynamicVolume for a car that is in the scene and deformed this frame, keyed by the WHOLE
# 64-bit mHandlingBodyVolumeId (entity word in the high dword, low dword 0 -- "key <entity>:0").
# Before the fix nothing posted it (0 lines; the box stayed AddToScene's pristine handling box).
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxRcem4PropBoxLive.ps1 --run-name fxrcem4_prop_box
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxrcem4_prop_box'
$case.Bug = 'A deformed race car kept its undeformed scene volume: UpdatePropBoundingBoxes_PreScene / GetPropCollisionBox had no body and no caller.'
$case.DiagEnv += ',BRN_PROP_BOX_DIAG=1'
$case.Run.MaxSeconds = 150
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'a deformed car''s volume is replaced, keyed by the whole 64-bit handle'
       Pattern = '\[prop-box\] slot \d+ key \d+:0 half'; Expect = $true }
    @{ Kind = 'LogCount'; Name = 'the scene finds every replaced volume, and it is dynamic'
       Pattern = "Can't find volume|Can't replace volume"; Max = 0 }
)
$case
