# FX-TAILS-A item 1 live case (crash parity 2026-09-24): the pad record's idle byte reaches the director.
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxTailsAPadIdleLive.ps1
# The chain: InputPads::Update @0x828F8690 stores +0x3A0 mbPadIdle (`stb r7, 0x3A0(r25)` @0x828F8CB0) = "no action
# held or released this frame" -> BridgeControllerToDirector @0x823C0F70 byte 0 = !idle (cntlzw, 0x823C0FC8..0x823C0FFC)
# -> MainDirector's tail: GameState +0x148 mfPadInactiveTime += step while byte 0 is clear -> ArbStateCarSelect
# @0x8226F5D0, E_STATE_ROTATE_ABOUT_CAR (9): mfPadInactiveTime > 10.0 (flt_82CDADA4) && mfTimeInState > 10.0 ->
# the idle orbit, E_STATE_IDLE (11).
# The run parks at car select (-HoldCarSelect: the harness stops its Accept pump there) and touches nothing, so the
# pad is idle; the witness is the director's own BRN_DIRECTOR_TRACE line `[carselect] meState -> 11`.
# Before (b5 db45f054 and earlier): CgsInputPadsPC.cpp stored +0x3A0 = 0 on every fill, the clock was zeroed every
# frame and car select never left state 9 however long it sat.
@{
  Name   = 'fxtailsa_pad_idle'
  Area   = 'input'
  Bug    = 'The PC pad fill wrote the idle byte (+0x3A0 mbPadIdle) as 0 on every frame, so the director saw input every frame: the pad-inactivity clock never ran and the junkyard car-select idle orbit (and the Picture Paradise idle entry) could never start.'
  Frames = $false
  Run    = @{
    HoldCarSelect = $true
    MaxSeconds    = 80
    SkipIntro     = $true
    AcceptGap     = 1.0
  }
  DiagEnv = 'BRN_DIRECTOR_TRACE=1,BRN_CAM_INPUT_DIAG=1'
  Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'LogMatch';   Name = 'car select reached its browse state (E_STATE_ROTATE_ABOUT_CAR, 9)';
       Pattern = '\[carselect\] meState -> 9 '; Expect = $true }
    @{ Kind = 'Script'; Name = 'with the pad idle, car select enters the idle orbit (9 -> 11, E_STATE_IDLE)'; Script = {
        param($ctx)
        $seq = @()
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[carselect\] meState -> (\d+) ') { $seq += [int]$Matches[1] }
        }
        $i9 = [array]::IndexOf($seq, 9)
        $i11 = -1
        if ($i9 -ge 0) { for ($k = $i9 + 1; $k -lt $seq.Count; $k++) { if ($seq[$k] -eq 11) { $i11 = $k; break } } }
        @{ Pass = ($i11 -gt $i9 -and $i9 -ge 0); Detail = ("carselect states: " + ($seq -join ' -> ')) }
    } }
  )
}
