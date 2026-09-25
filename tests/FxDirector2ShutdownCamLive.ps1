# FX-DIRECTOR2 (crash parity 2026-09-25) live case: a shutdown takedown plays its ICE shot (Takedown_ICE_Shut) with no
# assertion.
#
# ArbStateTakedown::Prepare's shutdown arm binds the takedown shot group's shot through SimpleIceTakedownPlayer::
# SetIceAnim @0x821F58C8. The console checks the shot's WHOLE u64 class key against 0x4644E379A997C1EE (`ld` + `cmpld`,
# 0x821F58F0..0x821F5904). The PC compared two dwords, which is never true of the real key on a little-endian host, so
# every shutdown takedown asserted and paused the game (fxdirector2_iceanim_bystander/20260925_220559).
# The shutdown arm runs for a free-roam rival shutdown (E_ACTION_SHUTDOWN) or under the console's own debug toggle
# "Always do shutdown TD camera". DebugComponent::OnActivate registers it over ArbStateTakedown +0x62C (string
# 0x8200D140), and BRN_ALWAYS_SHUTDOWN_CAM=1 raises it ([HARNESS], opt-in).
# The drive is RivalOrganic.ps1's Road Rage with the game's own AI on the pad (--ai-pad pursuit).
# Witnesses (NOT X360, BRN_CRASHCAM_DIAG):
#   [crashcam] shutdown takedown shot I of N take guid G
#   [crashcam] ice takedown ACTIVE|FINISHED|FAILED take guid G at T s
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxDirector2ShutdownCamLive.ps1 --ai-pad pursuit --no-frames --run-name fxdirector2_shutdown_cam
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxdirector2_shutdown_cam'
$case.Area = 'director'
$case.Bug = 'A shutdown takedown must bind its ICE shot without asserting: SimpleIceTakedownPlayer::SetIceAnim compares the whole u64 class key, as the console does.'
$case.DiagEnv += ',BRN_ALWAYS_SHUTDOWN_CAM=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions (no SetIceAnim class-key assert)' }
    @{ Kind = 'LogCount'; Name = 'no SetIceAnim class-key assert'; Pattern = 'lpIceAnim && lpIceAnim->GetClassKey'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'the console''s "Always do shutdown TD camera" toggle is on (harness)'; Pattern = '\[crashcam\] HARNESS-ONLY \(BRN_ALWAYS_SHUTDOWN_CAM=1\)'; Expect = $true }
    @{ Kind = 'LogCount'; Name = 'a takedown took the shutdown arm'; Pattern = '\[crashcam\] takedown latch .* alwaysShutdownCam=1'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the shot is Takedown_ICE_Shut (guid 554362)'; Pattern = '^\[crashcam\] shutdown takedown shot \d+ of \d+ take guid 554362'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'Takedown_ICE_Shut plays (the ICE takedown player went ACTIVE)'; Pattern = '^\[crashcam\] ice takedown ACTIVE take guid 554362'; Min = 1 }
    @{ Kind = 'Script'; Name = 'the ICE takedown takes (report)'; Script = {
        param($ctx)
        $rows = @($ctx.LogLines | Where-Object { $_ -match '^\[crashcam\] (ice takedown|shutdown takedown shot) ' } | ForEach-Object { $_ -replace ' \[FLAG PC witness\]', '' -replace '^\[crashcam\] ', '' })
        @{ Pass = $true; Detail = (($rows | Select-Object -First 12) -join ' | ') }
    } }
)
$case
