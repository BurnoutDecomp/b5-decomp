# FX-DIRECTOR2 (crash parity 2026-09-25) CC-12 live case: the crash camera's moment selector rebalances.
#
# ArbStateCrashing::Construct @0x82259EA0 registers four crash moments under a max-active limit of ONE:
#   slot 0  TUMBLING + TUMBLING_LEAD_CRASH_ONLY           (type 2, param 8), inhibitable
#   slot 1  TUMBLING + TUMBLING_TRUCKING_SIDE_CRASH_ONLY  (type 2, param 4), inhibitable
#   slot 2  HARD_STOP + HARD_STOP_DEFAULT                 (type 0, param 1)
#   slot 3  BYSTANDER_SEES_ACTION + BYSTANDER_FAR_CRASH_ONLY (type 5, param 3)
# Prepare starts slot 1 inhibited. MomentSelector::Update's rebalance (0x8223A41C..0x8223A660) swaps a ready held-back
# moment in for an idle running one. The PC had gated that tail, so the trucking-side tumbling shot could never swap
# in during a crash.
# The drive is FxDirectorMomentTickLive.ps1's crash sweep (the harbour rock-slope seat into the water, a 44 m/s run into
# the quay rocks, the slope seat again).
# Witness (NOT X360, BRN_CRASHCAM_DIAG): each rebalance move prints
#   [selector] rebalance <swap in|swap out|un-inhibit ...> slot S (moment type T param P)
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxDirector2SelectorRebalanceLive.ps1
$case = & (Join-Path $PSScriptRoot 'FxDirectorMomentTickLive.ps1')
$case.Name = 'fxdirector2_selector_rebalance'
$case.Bug = 'The crash camera''s moment selector must rebalance under its max-active limit of one (CC-12): a ready held-back moment swaps in for an idle running one.'
$case.Checks += @(
    @{ Kind = 'LogCount'; Name = 'CC-12: the rebalance moved a moment during a crash'; Pattern = '^\[selector\] rebalance '; Min = 1 }
    @{ Kind = 'Script'; Name = 'CC-12: the moves (slot, type, parameter)'; Script = {
        param($ctx)
        $moves = @($ctx.LogLines | Where-Object { $_ -match '^\[selector\] rebalance ' } | ForEach-Object { $_ -replace '^\[selector\] rebalance ', '' })
        $trucking = @($moves | Where-Object { $_ -match '(swap in|un-inhibit).*slot 1 \(moment type 2 param 4\)' })
        @{ Pass = ($moves.Count -gt 0)
           Detail = ("{0} move(s); trucking-side tumbling swapped in {1} time(s): {2}" -f $moves.Count, $trucking.Count, (($moves | Select-Object -First 12) -join ' | ')) }
    } }
)
$case
