# FX-VMNET (crash parity 2026-09-23) live witness for the VehicleManager HIDE_ONLINE leaves.
# The RivalOrganic flow crosses every one of them offline: the junkyard drive-thru in/out
# (action 99), the mode PREPARE (23) and START (34), and the harness teleport, which is a
# player-car reset through ProcessResetEvents. Offline the leaves change no gameplay (their only
# consumers act on NETWORK race cars), so what this proves is DISPATCH plus no new asserts: the
# witnesses are the console's own HIDE_ONLINE log strings, gated on gxMessageFilterFlags bit 0.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxVmnetHideOnlineLive.ps1
# RED side: -NoRun -RunDir <a pre-fix rival run> (the deferral lines are there, the witnesses not).
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxvmnet_hide_online'
$case.Area = 'network'
$case.Bug = 'The VehicleManager mode/junkyard/reset HIDE_ONLINE leaves and the network catch-up stage must be dispatched (G40-D2, G43-D2, G44-D1, G44-D2).'
# BRN_NETCATCHUP_DIAG arms the one-shot [net-catchup] witness in PhysicsModule::UpdateNetworkCatchup
# (G44-D2): the stage WorldModule::Update runs every frame; offline its driver queue holds no
# NETWORK record, so the witness is the only thing that shows the un-gated body ran.
$case.DiagEnv += ',BRN_NETCATCHUP_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'G40-D2 OnPrepareGameMode dispatched (0x82091300)'
       Pattern = 'HIDE_ONLINE: Just prepared an online game mode \(start forcing race cars to be visible when created\)'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'G40-D2 OnStartGameMode dispatched (0x820913B8)'
       Pattern = 'HIDE_ONLINE: Just started an online game mode \(stop forcing race cars to be visible when created\)'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'G43-D2 OnJunkYardDriveThru entry dispatched (0x82097264)'
       Pattern = 'HIDE_ONLINE: Player entered a junkyard, so making all network cars hidden'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'G43-D2 OnJunkYardDriveThru exit dispatched (0x82097218)'
       Pattern = 'HIDE_ONLINE: Player exitted a junkyard, so can start making network cars visible again'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'G44-D1 player-reset hide dispatched (0x82099D38)'
       Pattern = 'HIDE_ONLINE: Making all network race cars hidden for at least 1 frame because player car \d+ was just reset'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'G44-D1 per-event reset stream (0x82099E44)'
       Pattern = 'HIDE_ONLINE: Resetting race car \d+, type \d+'; Expect = $true }
    @{ Kind = 'LogCount'; Name = 'no VehicleManager post-scene deferral left'
       Pattern = '\[postscene-action\] DEFERRED: VehicleManager::'; Max = 0 }
    @{ Kind = 'LogMatch'; Name = 'G44-D2 PhysicsModule::UpdateNetworkCatchup dispatched (offline: 0 NETWORK records)'
       Pattern = '\[net-catchup\] PhysicsModule::UpdateNetworkCatchup LIVE: driver queue \d+ record\(s\), 0 NETWORK'; Expect = $true }
    @{ Kind = 'LogCount'; Name = 'G44-D2 the WorldLinkStubs boot gate is gone'
       Pattern = 'PhysicsModule::UpdateNetworkCatchup: inert'; Max = 0 }
)
$case
