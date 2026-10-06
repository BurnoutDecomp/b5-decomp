# Diagnostic completion, not an organically earned win: the original debug
# finish member ends the running event; downstream removal/effects remain real.
$case = & (Join-Path $PSScriptRoot 'PlaytestBoostMultiplierLive.ps1')
$case.Name = 'playtest_render_ai_retire_1005'
$case.Area = 'vfx'
$case.Bug = 'Removed opponent model slots must reset their boost effects and retire active emitter bindings.'
$case.Run.DebugFinishPos = 1
$case.Run.DebugFinishAt = 35
$case.Run.MaxSeconds = 105
$case.Run.FrameEvery = 60
$case.DiagEnv += ',BRN_EFFECT_RETIRE_DIAG=1,BRN_BOOSTLOC_DIAG=1,BRN_DEFORMLOC_DIAG=1'
$case.Checks = @(
    @{ Kind='NewAsserts'; Name='no new assertions' }
    @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
    @{ Kind='Mark'; Name='reached driving'; Phase='DRIVING' }
    @{ Kind='LogMatch'; Name='original debug finish stimulus ran'; Pattern='\[evt-finish\].*HARNESS DEBUG FINISH POSITION' }
    @{ Kind='LogMatch'; Name='actual opponent model removed'; Pattern='\[effects-retire\] model car=[1-7] .*new=0000000000000000' }
    @{ Kind='Script'; Name='removed opponent handles reach destroy and leave no active bound emitter'; Script={
        param($ctx)
        $owner = -1
        $finish = $false
        $handles = @{}
        $before = @{}
        $after = @{}
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[evt-finish\].*HARNESS DEBUG FINISH POSITION') { $finish=$true }
            if ($line -match '\[effects-retire\] model car=(\d+) .*new=0000000000000000') {
                $owner = if ($finish) { [int]$Matches[1] } else { -1 }
            } elseif ($line -match '\[effects-retire\] reset-end car=') { $owner=-1 }
            elseif ($owner -ge 1 -and $line -match '\[effects-retire\] stop handle=(\w+) next=(\w+) hash=(\w+) before=(\w+) after=(\w+)') {
                if (([Convert]::ToInt32($Matches[5],16) -band 0x10) -ne 0) {
                    $handles[$Matches[2]]=@{Car=$owner; Hash=$Matches[3]}
                }
            } elseif ($line -match '\[effects-retire\] dispatch (before|after) handle=(\w+) hash=(\w+).*activeForBinding=(\d+)') {
                $phase=$Matches[1]; $handle=$Matches[2]; $hash=$Matches[3]; $count=[int]$Matches[4]
                if ($handles.ContainsKey($handle) -and $handles[$handle].Hash -eq $hash) {
                    if ($phase -eq 'before') {$before[$handle]=$count} else {$after[$handle]=$count}
                }
            }
        }
        $bad=@($handles.Keys | Where-Object { -not $after.ContainsKey($_) -or $after[$_] -ne 0 })
        $live=@($handles.Keys | Where-Object { $before.ContainsKey($_) -and $before[$_] -gt 0 })
        @{Pass=($handles.Count -gt 0 -and $live.Count -gt 0 -and $bad.Count -eq 0);
          Detail="$($handles.Count) original KILL handles scoped inside removed opponent Reset; $($live.Count) had active emitters; missing/nonzero-after=[$($bad -join ',')]"}
    } }
)
$case
